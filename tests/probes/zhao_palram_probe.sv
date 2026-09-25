// zhao_palram_probe -- the PALRAM packet's one-change-at-a-time instrument.
//
// WHY THIS FILE EXISTS
// --------------------
// `zhao_geom_drawjob` holds `logic [383:0] pal_q [XFORMS]` with XFORMS = 256.
// Its own source comment (zhao_geom_drawjob.sv:229-234) asserts that this is
// "A REGISTERED READ WITH AN ENABLE, so the row holds for the whole draw and
// Quartus infers M10K rather than 98,304 flops." Quartus 17.0.2 does not. The
// measured baseline, `zhao_geom_drawjob@palram-base`, is 100,561 registers and
// ZERO block memory bits, with a 256:1 x 384-bit read multiplexer costing
// 65,280 LEs in the Multiplexer Restructuring table -- and `pal_q` appears in
// NO RAM inference message at all, neither inferred nor uninferred-with-a-
// reason. Quartus never considered it a RAM candidate and never said why.
//
// A causal answer to "why" is the packet's deliverable, and a causal answer
// needs ONE VARIABLE MOVED PER MEASUREMENT. Mapping the whole 523-line block
// per experiment costs 240 s and drags 33,914 ALUTs of unrelated logic through
// every row; worse, it makes "the thing I changed" and "the thing that moved"
// hard to tie together. This probe is the palette and nothing else, so each
// map isolates exactly one property of the memory description.
//
// This file is a PROBE, not production, and nothing composes it. It lives here
// for the same reason the ground-contact rule demands a committed pose probe:
// "a probe that does this was written once and thrown away, so its numbers are
// unreproducible -- commit the probe."
//
// HOW TO USE IT
// -------------
//   tools\quartus\palram_map_probe.ps1 -Label '@probe-v0'
//        -Module zhao_palram_probe -Sources 'tests/probes/zhao_palram_probe.sv'
//        -TopParameters VARIANT=0
//
// READ VARIANT 0 FIRST, ALWAYS. It is the probe's POSITIVE CONTROL: it is the
// production description copied verbatim, and it must reproduce the defect
// (0 block memory bits, a 256:1 x 384-bit mux). A probe whose control arm
// infers M10K is not measuring the same machine as the real block, and every
// number taken from its other arms is about something else. Check the control
// before believing any arm.
//
// THE ARMS. Each differs from ARM 0 in EXACTLY ONE PROPERTY:
//   0  production, verbatim: 12 x 32-bit partial-select writes in a for loop,
//      a LOOM_IDXW-wide (10-bit) write index into a 256-deep array, and the
//      write and the read sharing one always_ff.
//   1  the WRITE becomes a single full-word assignment. Address and block
//      structure unchanged.
//   2  the WRITE INDEX is narrowed to XIDXW (8) bits before it indexes the
//      array. Write form and block structure unchanged.
//   3  the read moves into its OWN always_ff. Write form and address unchanged.
//   4  arms 1 and 2 together -- only meaningful once 1 and 2 are each measured
//      alone, and present so the interaction can be read rather than assumed.
//
// Quartus 17.0 syntax law, both halves of it: explicit `generate`/`endgenerate`
// (an implicit generate is a syntax error there), and elaboration guards inside
// `initial begin ... end` (a module-scope `if` is a syntax error there, and
// `--lint-only` never runs the block, so neither is caught by a clean lint).
module zhao_palram_probe #(
    parameter int unsigned XFORMS    = 256,
    parameter int unsigned LOOM_IDXW = 10,
    parameter int unsigned VARIANT   = 0
) (
    input  var logic                  clk,
    input  var logic                  rst_n,

    // the write port, GEOM.LOOM's emit stream
    input  var logic                  px_valid_i,
    input  var logic [LOOM_IDXW-1:0]  px_index_i,
    input  var logic signed [31:0]    px_m_i [12],

    // the read port, the draw's transform id
    input  var logic                  rd_en_i,
    input  var logic [23:0]           xf_idx_i,

    output var logic [383:0]          pal_rd_o,
    output var logic                  xf_valid_o,
    output var logic [31:0]           pal_writes_o,
    output var logic [31:0]           pal_dropped_o
);

  localparam int unsigned XIDXW = (XFORMS <= 1) ? 1 : $clog2(XFORMS);

  initial begin
    if (XFORMS == 0)
      $fatal(1, "zhao_palram_probe: XFORMS must be at least one");
    if (VARIANT > 4)
      $fatal(1, "zhao_palram_probe: VARIANT is 0..4");
  end

  // The valid bits, IDENTICAL IN EVERY ARM so they are never the variable.
  // They are separate flops because reset must clear them, and an
  // asynchronous clear on the payload array would destroy its inference --
  // the argument the production comment makes, kept intact here.
  logic pal_v_q [XFORMS];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int unsigned r = 0; r < XFORMS; r++) pal_v_q[r] <= 1'b0;
      pal_writes_o  <= '0;
      pal_dropped_o <= '0;
    end else begin
      if (px_valid_i) begin
        if (32'(px_index_i) < 32'(XFORMS)) begin
          pal_v_q[int'(px_index_i)] <= 1'b1;
          pal_writes_o <= pal_writes_o + 32'd1;
        end else begin
          pal_dropped_o <= pal_dropped_o + 32'd1;
        end
      end
    end
  end

  wire xf_in_range_c = (xf_idx_i < 24'(XFORMS));
  assign xf_valid_o  = xf_in_range_c && pal_v_q[int'(xf_idx_i[XIDXW-1:0])];

  logic [383:0] pal_rd_q;
  assign pal_rd_o = pal_rd_q;

  generate
    if (VARIANT == 0) begin : g_v0_production
      // ARM 0 -- PRODUCTION, VERBATIM. The positive control.
      logic [383:0] pal_q [XFORMS];
      always_ff @(posedge clk) begin
        if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
          for (int unsigned k = 0; k < 12; k++)
            pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];
        end
        if (rd_en_i) pal_rd_q <= pal_q[int'(xf_idx_i[XIDXW-1:0])];
      end
    end else if (VARIANT == 1) begin : g_v1_wholeword
      // ARM 1 -- ONE CHANGE: the write is a single full-word assignment.
      logic [383:0] pal_q [XFORMS];
      wire [383:0] wr_word_c = {px_m_i[11], px_m_i[10], px_m_i[9], px_m_i[8],
                                px_m_i[7],  px_m_i[6],  px_m_i[5], px_m_i[4],
                                px_m_i[3],  px_m_i[2],  px_m_i[1], px_m_i[0]};
      always_ff @(posedge clk) begin
        if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
          pal_q[int'(px_index_i)] <= wr_word_c;
        end
        if (rd_en_i) pal_rd_q <= pal_q[int'(xf_idx_i[XIDXW-1:0])];
      end
    end else if (VARIANT == 2) begin : g_v2_narrowaddr
      // ARM 2 -- ONE CHANGE: the write index is narrowed to XIDXW bits, so the
      // array is indexed by an address that cannot leave its own range. The
      // REFUSAL IS UNCHANGED: the enable still carries the full-width
      // comparison, so an out-of-range node is still dropped and counted by
      // pal_dropped_o, never wrapped into another instance's row.
      logic [383:0] pal_q [XFORMS];
      wire [XIDXW-1:0] wr_addr_c = px_index_i[XIDXW-1:0];
      always_ff @(posedge clk) begin
        if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
          for (int unsigned k = 0; k < 12; k++)
            pal_q[wr_addr_c][32 * k +: 32] <= px_m_i[k];
        end
        if (rd_en_i) pal_rd_q <= pal_q[int'(xf_idx_i[XIDXW-1:0])];
      end
    end else if (VARIANT == 3) begin : g_v3_splitblocks
      // ARM 3 -- ONE CHANGE: the read gets its own always_ff.
      logic [383:0] pal_q [XFORMS];
      always_ff @(posedge clk) begin
        if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
          for (int unsigned k = 0; k < 12; k++)
            pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];
        end
      end
      always_ff @(posedge clk) begin
        if (rd_en_i) pal_rd_q <= pal_q[int'(xf_idx_i[XIDXW-1:0])];
      end
    end else begin : g_v4_wholeword_narrowaddr
      // ARM 4 -- arms 1 and 2 together.
      logic [383:0] pal_q [XFORMS];
      wire [XIDXW-1:0] wr_addr_c = px_index_i[XIDXW-1:0];
      wire [383:0] wr_word_c = {px_m_i[11], px_m_i[10], px_m_i[9], px_m_i[8],
                                px_m_i[7],  px_m_i[6],  px_m_i[5], px_m_i[4],
                                px_m_i[3],  px_m_i[2],  px_m_i[1], px_m_i[0]};
      always_ff @(posedge clk) begin
        if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
          pal_q[wr_addr_c] <= wr_word_c;
        end
        if (rd_en_i) pal_rd_q <= pal_q[int'(xf_idx_i[XIDXW-1:0])];
      end
    end
  endgenerate

endmodule
