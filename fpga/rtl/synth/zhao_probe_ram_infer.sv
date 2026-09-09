// zhao_probe_ram_infer -- v4. IS THE BLOCKER THE READ ADDRESS'S PROVENANCE?
//
// ---------------------------------------------------------------------------
// FOUR CANDIDATES ARE ALREADY DEAD
// ---------------------------------------------------------------------------
// zhao_raster_perspuv_svc is the texture island's largest register consumer:
// 3,240 registers, 4.63x its S3.3 budget, and 85% of that is a 16-entry token
// table. Exactly one of its arrays became an M10K (e_tag, 16x14). e_num_u --
// same depth, one write address at tail_q, one read address, not reset-cleared
// -- did not. Eliminated so far:
//
//   1. READ-ADDRESS COUNT   refuted by the @g2-prod fit: after the 2026-09-06
//                           per-axis split e_num_u has exactly one of each and
//                           still does not infer.
//   2. THE ARRAY RESET      refuted by reading perspuv's reset branch: it
//                           clears ONLY e_val and e_have, which are also the
//                           two arrays with multiple write addresses. e_num_u
//                           is not cleared at all.
//   3. READ STYLE           refuted by v3: a read inside always_ff at a
//                           combinational index inferred fine (arr_r).
//   4. WIDTH                refuted by v3: 14 and 32 both inferred.
//
// v3 is the reason 3 and 4 are dead, and it is a validated instrument -- all
// three of its arrays inferred, its positive control fired, and nothing merged
// once each array had its own write-data port.
//
// So every SHAPE in perspuv's token table infers in isolation, which means the
// blocker is something perspuv does that the probe has not yet reproduced. That
// is an encouraging result rather than a dead end: it makes the table look more
// convertible, not less, and ~3,000 registers of a 7,285-register overage is
// worth one more 80-second map.
//
// ---------------------------------------------------------------------------
// THE REMAINING DIFFERENCE: WHERE THE READ ADDRESS COMES FROM
// ---------------------------------------------------------------------------
// In every probe so far the read address arrived on a PORT. In perspuv it does
// not -- it is itself the output of another array read:
//
//     logic [TW-1:0] pk_i [2];
//     ...
//     pk_i[ax] = wq[ax][wq_rp[ax][TW-1:0]];      // a combinational array read
//     ...
//     p0_num_q[ax] <= (ax == 0) ? e_num_u[pk_i[0]] : e_num_v[pk_i[1]];
//
// So the address path into e_num_u runs through a second dynamic-index read of
// `wq`. A memory's address port has to be a real address; an address that is
// itself deep combinational logic out of another array may be what stops the
// inference.
//
// ---------------------------------------------------------------------------
// THE DESIGN: two arrays, one factor, one shared control
// ---------------------------------------------------------------------------
//   P  read in always_ff at an address from a PORT           <- v3's arr_r,
//                                                              KNOWN to infer
//   R  read in always_ff at an address that is itself a
//      combinational read of the small array `wq`            <- the perspuv shape
//
// Both 16 x 32, one write address, no reset clear, and each with its own
// write-data port so neither can be merged into the other (v1 and v2 were both
// ruined by merging; constant salts cannot prevent it because merging is
// per bit).
//
// READING IT:
//
//   P infers, R does not  -> THE ADDRESS'S PROVENANCE DECIDES. The remedy is
//                            then concrete and local: register the address one
//                            cycle before the array read, and perspuv's token
//                            table can become memory.
//   both infer            -> address provenance is innocent too, and the
//                            remaining suspects are the CONDITIONAL and the
//                            for-loop ternary wrapping perspuv's read, or
//                            something outside the table entirely.
//   P does not infer      -> THE PROBE IS BROKEN. v3 proved this exact shape
//                            infers, so a failure here means the harness
//                            changed, not the design. Check this FIRST.
//
// `wq` is deliberately a real array and not a register: making the address come
// from a flop would be testing the remedy, not the hypothesis.

`default_nettype none

module zhao_probe_ram_infer #(
    parameter int unsigned DEPTH = 16,
    parameter int unsigned WIDE = 32,
    // Derived, but a parameter and not a localparam because the ports use it.
    parameter int unsigned AW = $clog2(DEPTH)
) (
    input  wire                clk,
    input  wire                rst_n,

    input  wire                wr_en,
    input  wire [AW-1:0]       wr_addr,
    // One write-data port per array; this is what stops register merging.
    input  wire [WIDE-1:0]     wr_data_p,
    input  wire [WIDE-1:0]     wr_data_r,

    // the address-source array, written from its own port
    input  wire                wq_we,
    input  wire [AW-1:0]       wq_waddr,
    input  wire [AW-1:0]       wq_wdata,
    input  wire [AW-1:0]       wq_raddr,

    input  wire [AW-1:0]       rd_addr_c,
    input  wire                rd_en,

    output logic [WIDE-1:0]    p_o,
    output logic [WIDE-1:0]    r_o
);

  logic [WIDE-1:0] arr_p [DEPTH];
  logic [WIDE-1:0] arr_r [DEPTH];

  // The address-source array. perspuv's `wq`, minimally.
  logic [AW-1:0]   wq [DEPTH];

  // THE ADDRESS UNDER TEST: itself a combinational dynamic-index array read,
  // exactly as perspuv's `pk_i[ax] = wq[ax][wq_rp[ax]]` is.
  logic [AW-1:0]   pk_i_c;
  assign pk_i_c = wq[wq_raddr];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p_o <= '0;
      r_o <= '0;
    end else if (rd_en) begin
      // P: address straight from a port. v3 proved this infers.
      p_o <= arr_p[rd_addr_c];
      // R: address out of another array read. The only difference.
      r_o <= arr_r[pk_i_c];
    end
  end

  // ---- writes. NO RESET IN THIS PROCESS AT ALL. ---------------------------
  // The first draft had `arr_p[0] <= arr_p[0];` as a placeholder to avoid an
  // empty reset branch. That is a WRITE AT A SECOND ADDRESS -- a constant one --
  // and a second write address is exactly the property that makes an array
  // un-inferable (S5.3 forbids it by name, and it is why perspuv's e_val,
  // e_sat and e_have can never be memory). It would have confounded this
  // experiment in the direction of "did not infer" and looked like a finding.
  //
  // Caught by reading the draft back before spending the map. A reset-free
  // always_ff is both correct here and closer to perspuv, whose token arrays
  // are not reset-cleared either.
  always_ff @(posedge clk) begin
    if (wr_en) begin
      arr_p[wr_addr] <= wr_data_p;
      arr_r[wr_addr] <= wr_data_r;
    end
    if (wq_we) wq[wq_waddr] <= wq_wdata;
  end

endmodule

`default_nettype wire
