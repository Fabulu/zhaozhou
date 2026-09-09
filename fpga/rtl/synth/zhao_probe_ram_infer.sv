// zhao_probe_ram_infer -- v3. DOES READ STYLE DECIDE RAM INFERENCE?
//
// ---------------------------------------------------------------------------
// THE QUESTION, NARROWED TWICE BY MEASUREMENT
// ---------------------------------------------------------------------------
// In zhao_texture_island_v3_top@g2-prod, zhao_raster_perspuv_svc holds a
// 16-entry token table that is 85% of its 3,240 registers -- the island's
// largest single register consumer, 4.63x its S3.3 register budget. Exactly ONE
// of its arrays became an M10K:
//
//     e_tag    16 x 14   INFERRED, Simple Dual Port
//     e_num_u  16 x 32   NOT inferred
//     e_mant_u 16 x 24   NOT inferred
//
// Two candidate explanations have now been eliminated:
//
//   READ-ADDRESS COUNT. The 2026-09-06 per-axis split was built on it. After
//   that split e_num_u has one write address and one read address, and it still
//   does not infer. Refuted by the @g2-prod fit.
//
//   THE ARRAY RESET. v1/v2 of this probe found that the only variant to infer
//   was the only one not cleared on reset -- but perspuv's reset branch clears
//   ONLY e_val and e_have (lines 434-435), and those are the two arrays with
//   multiple write addresses anyway. e_num_u, e_mant_u, e_k, e_q_u and e_tag are
//   NOT reset-cleared, so e_num_u already has the property that made v2's arr_d
//   infer. Refuted by reading the RTL, which cost nothing.
//
// What is left, and all this probe now tests, is READ STYLE:
//
//     e_tag    assign ob_tag_c = e_tag[head_q];      continuous, registered idx
//     e_num_u  p0_num_q[ax] <= e_num_u[pk_i[0]];     inside always_ff,
//                                                     combinational index
//
// ---------------------------------------------------------------------------
// WHAT v1 AND v2 GOT WRONG, AND WHY IT IS FIXED THIS WAY
// ---------------------------------------------------------------------------
// v1 wrote all five variants identically. Quartus merged four of them:
//
//     arr_b[i][b]  Merged with  arr_a[i][b]     224 registers
//     arr_c[i][b]  Merged with  arr_a[i][b]     224 registers
//     arr_e[i][b]  Merged with  arr_a[i][b]     224 registers
//
// and the survivor carried the UNION of their read sites -- four addresses --
// so it could not be dual-port whatever else was true of it. That is the same
// mechanism that undid perspuv's e_mant split (384 registers merged back
// because both copies came from one source on one clock), reproduced minimally,
// and it is why v1's result could not be attributed.
//
// v2 tried a per-variant XOR constant. It did not work, and the reason is worth
// keeping: MERGING IS PER BIT. Salts differing only in bits 0-4 leave bits 5-31
// provably equal, so arr_c still lost 496 rows. With five variants no set of
// constants can differ pairwise in every bit -- two values per bit position,
// pigeonhole.
//
// v3 therefore gives each array ITS OWN WRITE-DATA PORT. Two arrays fed by
// unrelated inputs cannot be proved equal at any bit, so none can be merged.
//
// ---------------------------------------------------------------------------
// THE DESIGN: three arrays, ONE factor
// ---------------------------------------------------------------------------
// All 16 deep, one write address, one read address, and NONE cleared on reset --
// reset is eliminated as a variable because it is already ruled out for the
// island.
//
//   P  14 wide  continuous assign @ registered index   <- the e_tag shape
//   Q  32 wide  continuous assign @ registered index   <- P, widened
//   R  32 wide  read in always_ff @ comb index         <- the e_num_u shape
//
// READING IT:
//
//   P and Q infer, R does not  -> READ STYLE DECIDES. Actionable: perspuv's
//                                 token-table reads move to continuous assigns
//                                 feeding flops, and ~3,000 registers could
//                                 become M10K.
//   P infers, Q does not       -> WIDTH decides and read style is innocent.
//   all three infer            -> the blocker is something in perspuv this
//                                 probe still does not reproduce. Next
//                                 candidate: the read index's provenance --
//                                 perspuv's pk_i comes out of ANOTHER array
//                                 read (wq), not from a port.
//   P does not infer           -> THE PROBE IS BROKEN. P reproduces the one
//                                 array known to infer; if it fails, nothing
//                                 else here is evidence. Check this FIRST.
//
// EVERY READ REACHES A PORT, or the array is deleted before inference runs and
// "did not infer" would mean nothing.

`default_nettype none

module zhao_probe_ram_infer #(
    parameter int unsigned DEPTH = 16,
    parameter int unsigned NARROW = 14,
    parameter int unsigned WIDE = 32,
    // Derived, but a parameter and not a localparam because the ports use it.
    parameter int unsigned AW = $clog2(DEPTH)
) (
    input  wire                clk,
    input  wire                rst_n,

    input  wire                wr_en,
    input  wire [AW-1:0]       wr_addr,
    // ONE WRITE-DATA PORT PER ARRAY. This is what stops register merging; see
    // the header. Sharing one port is what broke v1 and v2.
    input  wire [NARROW-1:0]   wr_data_p,
    input  wire [WIDE-1:0]     wr_data_q,
    input  wire [WIDE-1:0]     wr_data_r,

    input  wire                rd_adv,
    input  wire [AW-1:0]       rd_addr_c,
    input  wire                rd_en,

    output wire [NARROW-1:0]   p_o,
    output wire [WIDE-1:0]     q_o,
    output logic [WIDE-1:0]    r_o
);

  // The registered read pointer, mirroring perspuv's head_q.
  logic [AW-1:0] rptr_q;

  logic [NARROW-1:0] arr_p [DEPTH];
  logic [WIDE-1:0]   arr_q [DEPTH];
  logic [WIDE-1:0]   arr_r [DEPTH];

  // ---- P: the e_tag shape. Continuous assign at a REGISTERED index. --------
  // POSITIVE CONTROL. If this does not infer, the probe does not reproduce the
  // island and no other row here is evidence.
  assign p_o = arr_p[rptr_q];

  // ---- Q: P widened to 32. Isolates width from read style. ----------------
  assign q_o = arr_q[rptr_q];

  // ---- R: the e_num_u shape. Read inside always_ff at a COMB index. -------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) r_o <= '0;
    else if (rd_en) r_o <= arr_r[rd_addr_c];
  end

  // ---- writes. NO ARRAY IS CLEARED ON RESET. ------------------------------
  // Deliberate: reset is ruled out as the island's blocker (perspuv clears only
  // e_val and e_have), so leaving it out removes a variable rather than
  // reintroducing v1's confound. Only rptr_q is reset, because a pointer must
  // start somewhere.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rptr_q <= '0;
    end else begin
      if (rd_adv) rptr_q <= rptr_q + AW'(1);
      if (wr_en) begin
        arr_p[wr_addr] <= wr_data_p;
        arr_q[wr_addr] <= wr_data_q;
        arr_r[wr_addr] <= wr_data_r;
      end
    end
  end

endmodule

`default_nettype wire
