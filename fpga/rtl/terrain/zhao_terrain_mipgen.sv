// zhao_terrain_mipgen.sv — the page height mips, by NESTED DECIMATION.
//
// ---------------------------------------------------------------------------
// THE LAW, AND WHY IT IS A SELECTION AND NOT A FILTER
// ---------------------------------------------------------------------------
// Owner ruling T8 (2026-09-02), verbatim and complete:
//
//     mip17[i,j] = fine33[2*i, 2*j]   i,j in 0..16
//     mip9 [i,j] = fine33[4*i, 4*j]   i,j in 0..8
//
//   > Top and bottom. (17*17 + 9*9) * 2 B * 2 surfaces = 1,480 B in a
//   > 1,536-byte record. NESTED DECIMATION KEEPS SHARED VERTICES EXACT AND
//   > INTRODUCES NO ROUNDING.
//
// There is no averaging, no filter kernel and no arithmetic on the samples at
// all. A coarse vertex IS a fine vertex, bit for bit, and that is the property
// the whole thing exists for: two patches at different LOD that share an edge
// must agree on that edge EXACTLY, or the seam cracks. An averaging mip cannot
// promise that; a decimating one cannot break it.
//
// So this block does not compute. It selects, and the only interesting thing
// in it is the addressing.
//
// ---------------------------------------------------------------------------
// WHICH MEANS IT NEEDS NO STORAGE
// ---------------------------------------------------------------------------
// The obvious shape is "buffer the 33x33 lattice, then walk it twice". That
// would be 1,089 samples x 2 surfaces of on-chip memory to produce a result
// that depends on no sample but the one in hand.
//
// Because both mips are strided subsets of the SAME scan, each fine sample can
// be routed as it arrives: to mip17 when both its coordinates are even, and to
// mip9 when both are multiples of four. Nested decimation means every mip9
// vertex is also a mip17 vertex, so the second condition implies the first --
// which is the same statement as "shared vertices are exact", visible here as
// a bit test rather than as a promise.
//
// ---------------------------------------------------------------------------
// SCOPE
// ---------------------------------------------------------------------------
// T8 also says the same law generates mips for a live composed lattice into
// COMPOSED_MIP_POOL, and that HPS does NOT implement a second mip law. This
// block is that one law; the composed pool is the same block on a different
// source, not a second implementation.
//
// It does not verify CRCs and does not decide residency. T8's ordering -- "a
// page becomes RESIDENT only after: payload CRC passes; bytes complete;
// resident mips complete" -- is the residency directory's to enforce, and this
// block's `done_o` is one of its three inputs.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_terrain_mipgen #(
    // The fine lattice is 33x33. Not a parameter with a default that happens
    // to be 33: the LAW above names 33, 17 and 9, and a different edge length
    // is a different law with different shared vertices.
    parameter int unsigned FINE   = 33,
    parameter int unsigned SURFS  = 2,    // top and bottom
    // The directory's identity widths, so a completion from here can be
    // matched against the entry that asked for it. Defaults are
    // zhao_terrain_residency_v2's own (256 sets x 4 ways, generation u8 per
    // ruling T10); they are parameters rather than imports because this block
    // does not otherwise know what a slot IS.
    parameter int unsigned SLOTW  = 10,
    parameter int unsigned GENW   = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- control -------------------------------------------------------------
    // A pulse restarts the scan. Anything in flight is abandoned: a page whose
    // load was aborted must not contribute half a mip to the next one.
    input  var logic          start_i,
    output var logic          busy_o,
    output var logic          done_o,     // one pulse, after the last sample

    // ---- WHICH PAGE THIS SCAN WAS FOR ---------------------------------------
    // `done_o` alone was a bare pulse with no page identity, and the comment
    // above already said it "is one of [the directory's] three inputs" -- but
    // TERRAIN.RESIDENCY matches a completion on {slot, gen, epoch}, so a pulse
    // it cannot attribute is a pulse it cannot act on.
    //
    // The consequence was measured by the composed terrain test on 2026-09-07,
    // not inferred: a claim sets `mips_stale`, so the loader's `fin` parks the
    // entry in ST_MIPGEN and only a SECOND completion clears it. With no
    // second completion in the tree, EIGHT PAGES were fetched, CRC-verified
    // and byte-identical in their slots while `resident_o` stayed at ZERO.
    // The assembled machine could not turn a loaded page into ground.
    //
    // The identity is CAPTURED AT `start_i` and returned unaltered. This block
    // does not interpret it -- it has no business knowing what a slot means,
    // and a scan that could rewrite the identity it was given would be a second
    // source of truth, which is the same reason the fine port carries no
    // coordinate.
    input  var logic [SLOTW-1:0] job_slot_i,
    input  var logic [GENW-1:0]  job_gen_i,
    input  var logic [31:0]      job_epoch_i,

    output var logic [SLOTW-1:0] done_slot_o,
    output var logic [GENW-1:0]  done_gen_o,
    output var logic [31:0]      done_epoch_o,

    // ---- the fine lattice, in scan order -------------------------------------
    // Row-major within a surface, surfaces in order. The order IS the address:
    // there is no coordinate on this port, because a coordinate that can
    // disagree with the scan is a second source of truth.
    input  var logic          fine_valid_i,
    output var logic          fine_ready_o,
    input  var logic [15:0]   fine_h_i,   // height16, S 1.7.8

    // ---- mip 17x17 -----------------------------------------------------------
    output var logic          m17_valid_o,
    output var logic [8:0]    m17_addr_o,   // 0..288 within a surface
    output var logic [$clog2(SURFS)-1:0] m17_surf_o,
    output var logic [15:0]   m17_h_o,

    // ---- mip 9x9 -------------------------------------------------------------
    output var logic          m9_valid_o,
    output var logic [6:0]    m9_addr_o,    // 0..80 within a surface
    output var logic [$clog2(SURFS)-1:0] m9_surf_o,
    output var logic [15:0]   m9_h_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]   samples_o,
    output var logic [31:0]   m17_writes_o,
    output var logic [31:0]   m9_writes_o,
    // A scan that was restarted before it finished. Not an error here -- an
    // aborted load is legal (T11) -- but it is the difference between a page
    // whose mips are complete and one whose mips merely exist.
    output var logic [31:0]   aborts_o
);

  localparam int unsigned C17 = (FINE + 1) / 2;   // 17
  localparam int unsigned C9  = (FINE + 3) / 4;   // 9
  localparam int unsigned CW  = $clog2(FINE);     // column/row counter width
  localparam int unsigned SW  = $clog2(SURFS);

  logic [CW-1:0] col_q, row_q;
  logic [SW-1:0] surf_q;
  logic          busy_q;

  assign busy_o       = busy_q;
  // Storage-free: this block never withholds a sample, because it never has
  // anywhere to put one. Readiness is occupancy of nothing.
  assign fine_ready_o = busy_q;

  logic take;
  assign take = busy_q && fine_valid_i && fine_ready_o;

  // ---- the two selections, as bit tests ------------------------------------
  logic sel17_c, sel9_c;
  assign sel17_c = (col_q[0] == 1'b0) && (row_q[0] == 1'b0);
  assign sel9_c  = (col_q[1:0] == 2'b00) && (row_q[1:0] == 2'b00);

  // The row stride is the mip's own edge length, written as C17 and C9 rather
  // than as 17 and 9. A hand-encoded `(r << 4) + r` is the same hardware and
  // one edit away from disagreeing with the dimension it is supposed to be --
  // multiplying by a CONSTANT is shift-and-add to any synthesiser, so nothing
  // is bought by spelling it out and a constraint is lost.
  //
  // No DSP either way: this block performs no arithmetic on heights at all.
  logic [8:0] a17_c;
  logic [6:0] a9_c;
  always_comb begin
    a17_c = 9'(row_q[CW-1:1] * CW'(C17)) + 9'(col_q[CW-1:1]);
    a9_c  = 7'(row_q[CW-1:2] * CW'(C9))  + 7'(col_q[CW-1:2]);
  end

  logic last_col, last_row, last_surf;
  assign last_col  = (col_q  == CW'(FINE - 1));
  assign last_row  = (row_q  == CW'(FINE - 1));
  assign last_surf = (surf_q == SW'(SURFS - 1));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      col_q <= '0; row_q <= '0; surf_q <= '0;
      busy_q <= 1'b0;
      done_o <= 1'b0;
      done_slot_o  <= '0;
      done_gen_o   <= '0;
      done_epoch_o <= '0;
      m17_valid_o <= 1'b0;
      m9_valid_o  <= 1'b0;
      samples_o <= '0; m17_writes_o <= '0; m9_writes_o <= '0; aborts_o <= '0;
    end else begin
      done_o      <= 1'b0;
      m17_valid_o <= 1'b0;
      m9_valid_o  <= 1'b0;

      if (start_i) begin
        // A restart mid-scan abandons the partial result and says so.
        if (busy_q) aborts_o <= aborts_o + 32'd1;
        // The identity travels with the scan it belongs to. Capturing it here
        // rather than reading the ports at `done_o` is the ingress-capture
        // rule: by the time the scan finishes, thousands of clocks later, the
        // ports belong to whichever page was queued next.
        done_slot_o  <= job_slot_i;
        done_gen_o   <= job_gen_i;
        done_epoch_o <= job_epoch_i;
        col_q  <= '0;
        row_q  <= '0;
        surf_q <= '0;
        busy_q <= 1'b1;
      end else if (take) begin
        samples_o <= samples_o + 32'd1;

        if (sel17_c) begin
          m17_valid_o  <= 1'b1;
          m17_addr_o   <= a17_c;
          m17_surf_o   <= surf_q;
          m17_h_o      <= fine_h_i;
          m17_writes_o <= m17_writes_o + 32'd1;
        end
        if (sel9_c) begin
          m9_valid_o  <= 1'b1;
          m9_addr_o   <= a9_c;
          m9_surf_o   <= surf_q;
          m9_h_o      <= fine_h_i;
          m9_writes_o <= m9_writes_o + 32'd1;
        end

        // ---- advance the scan ---------------------------------------------
        if (!last_col) begin
          col_q <= col_q + CW'(1);
        end else begin
          col_q <= '0;
          if (!last_row) begin
            row_q <= row_q + CW'(1);
          end else begin
            row_q <= '0;
            if (!last_surf) begin
              surf_q <= surf_q + SW'(1);
            end else begin
              busy_q <= 1'b0;
              done_o <= 1'b1;
            end
          end
        end
      end
    end
  end

endmodule : zhao_terrain_mipgen

`default_nettype wire
