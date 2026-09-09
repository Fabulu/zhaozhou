// zhao_texture_metajoin.sv
//
// ---------------------------------------------------------------------------
// THE RESPONSE METADATA BANK: ONE WRITER, ONE SYNCHRONOUS READER
// ---------------------------------------------------------------------------
// Post-fit brief section 5. This is the storage half of the metadata join --
// written once when a sample is planned, read once on the COMMON response
// stream before it splits into CLUT / nearest / bilinear.
//
// WHAT IT REPLACES, and the port count is the point.
// Today the island reads five asynchronous response-side ports across three
// tables:
//
//   sampmeta_m[64][3], 21 b   read by bilinear, CLUT and nearest
//   palslot_m[64],      2 b   read by the palette lookup
//   palgen_m[64],       8 b   read by the palette lookup
//
// The last two are the "64-owner palette binding selection" sitting on the
// island's worst INTERNAL path (rsp_dispatch|cq_rp -> palette_res|cold_o,
// -2.093 ns, 41 ps behind the nominal worst). A join carrying only the 21-bit
// sampmeta row would widen three queues, change a storage structure, cost a fit
// and LEAVE THAT PATH EXACTLY WHERE IT IS. That mistake was proposed, caught by
// the brief, and is why this record is 40 bits and not 21.
//
// WHY 256 x 40. Address is {owner_slot[5:0], sample_index[1:0]}: 256 rows, and
// the record is 40 bits. 256 x 40 = 10,240 bits = EXACTLY one M10K. The shape
// is chosen so the joined table is one block, not one and a fraction.
//
// SAMPLE INDEX 3 IS REPRESENTABLE AND ILLEGAL. The address space encodes it;
// the sample protocol does not permit it. A read at sidx 3 is refused here
// rather than returning a row nobody wrote -- an unwritten row is not a benign
// zero, it is whatever a previous owner left at that address.
module zhao_texture_metajoin #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned SIDXW = 2,
    parameter int unsigned GENW  = 8,
    parameter int unsigned PALSW = 2,
    parameter int unsigned METAW = 40
) (
    input  var logic                 clk,
    input  var logic                 rst_n,

    // ---- WRITE: one writer, at planned-sample acceptance --------------------
    input  var logic                 wr_valid_i,
    input  var logic [SLOTW-1:0]     wr_slot_i,
    input  var logic [SIDXW-1:0]     wr_sidx_i,
    input  var logic [GENW-1:0]      wr_owner_gen_i,
    input  var logic [PALSW-1:0]     wr_pal_slot_i,
    input  var logic [GENW-1:0]      wr_pal_gen_i,
    input  var logic [2:0]           wr_format_i,
    input  var logic [7:0]           wr_frac_u_i,
    input  var logic [7:0]           wr_frac_v_i,
    input  var logic                 wr_byte_sel_i,
    input  var logic                 wr_nibble_i,

    // ---- READ: one synchronous port, on the common response stream ----------
    // The result appears the NEXT cycle with rd_result_valid_o. That registered
    // boundary is what makes this a RAM read instead of a selection cone, which
    // is the entire point of moving the join here.
    input  var logic                 rd_valid_i,
    input  var logic [SLOTW-1:0]     rd_slot_i,
    input  var logic [SIDXW-1:0]     rd_sidx_i,
    input  var logic [GENW-1:0]      rd_owner_gen_i,

    output var logic                 rd_result_valid_o,
    output var logic [PALSW-1:0]     rd_pal_slot_o,
    output var logic [GENW-1:0]      rd_pal_gen_o,
    output var logic [2:0]           rd_format_o,
    output var logic [7:0]           rd_frac_u_o,
    output var logic [7:0]           rd_frac_v_o,
    output var logic                 rd_byte_sel_o,
    output var logic                 rd_nibble_o,
    // D0d, CLOSED. The row has always STORED the owner generation and the block
    // has always differenced it internally into `rd_gen_mismatch_o` -- but there
    // was no way out, so the island packed a literal `8'd0` where the identity
    // would travel and no downstream stage could notice a stale join.
    //
    // The port is only meaningful BECAUSE of the D0 repair: `rd_gen_q` is now
    // captured inside the same gate as the row, so what leaves here belongs to
    // the same read as the data beside it. Before that gate it tracked whatever
    // address was being offered, and exporting it would have exported a lie.
    output var logic [GENW-1:0]      rd_owner_gen_o,

    // ---- evidence -----------------------------------------------------------
    // rd_gen_mismatch_o is the alignment check the descriptor generation exists
    // to perform: the row was written for a generation, the response came back
    // naming one, and they must agree. A mismatch means a slot was recycled
    // under a response still in flight. That is a REAL fault, so it is COUNTED
    // and the read still returns, rather than being suppressed into a plausible
    // colour.
    output var logic [31:0]          writes_o,
    output var logic [31:0]          reads_o,
    output var logic [31:0]          rd_illegal_sidx_o,
    output var logic [31:0]          rd_gen_mismatch_o
);

  localparam int unsigned ROWS  = 1 << (SLOTW + SIDXW);
  localparam int unsigned ADDRW = SLOTW + SIDXW;

  // ---- THE RECORD LAYOUT, NAMED ONCE --------------------------------------
  // Every field offset below is DERIVED, not typed. The first version of this
  // file hand-wrote them:
  //
  //   format [20:18]  frac_v [17:10]  frac_u [9:2]  byte_sel [1]  nibble [0]
  //
  // and ALL FIVE were off by one -- the record is 40 bits and those slices
  // occupy 21, leaving bit 21 stranded and no room for the reserved bit. Lint
  // passed. Every output had a driver. Every field would have read the wrong
  // bits.
  //
  // That is the SIXTH instance of this defect class in this session, after the
  // five stale slices the v3own re-key produced -- and I wrote it hours after
  // adding `zhao_texture_ident_pkg` specifically to stop hand-slicing. The
  // lesson is not "be careful"; it is that offsets must be COMPUTED from the
  // widths, so that changing a width moves every field automatically.
  localparam int unsigned RSVD_W = 1;
  localparam int unsigned NIB_LO   = RSVD_W;
  localparam int unsigned BSEL_LO  = NIB_LO + 1;
  localparam int unsigned FRACU_LO = BSEL_LO + 1;
  localparam int unsigned FRACV_LO = FRACU_LO + 8;
  localparam int unsigned FMT_LO   = FRACV_LO + 8;
  localparam int unsigned PGEN_LO  = FMT_LO + 3;
  localparam int unsigned PSLOT_LO = PGEN_LO + GENW;
  localparam int unsigned OGEN_LO  = PSLOT_LO + PALSW;

  // If the arithmetic and the declared width ever disagree, stop at
  // elaboration rather than shipping a quietly misaligned record.
  // `initial`, not a bare module-scope `if`. Verilator accepts the bare
  // form; QUARTUS 17.0 REJECTS IT -- "syntax error near text: if; expecting
  // endmodule". Lint-clean is not the same as synthesizable, and this cost
  // a 33-second failed fit to discover.
  initial begin
    if (OGEN_LO + GENW != METAW)
      $fatal(1, "zhao_texture_metajoin: field offsets do not fill METAW");
  end

  logic [METAW-1:0] mem_q [ROWS];

  wire [ADDRW-1:0] wr_addr_c = {wr_slot_i, wr_sidx_i};
  wire [ADDRW-1:0] rd_addr_c = {rd_slot_i, rd_sidx_i};

  wire wr_legal_c = (wr_sidx_i != SIDXW'(3));
  wire rd_legal_c = (rd_sidx_i != SIDXW'(3));

  logic [METAW-1:0] rd_q;
  logic             rd_v_q;
  logic [GENW-1:0]  rd_gen_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_v_q            <= 1'b0;
      rd_gen_q          <= {GENW{1'b0}};
      writes_o          <= 32'd0;
      reads_o           <= 32'd0;
      rd_illegal_sidx_o <= 32'd0;
      rd_gen_mismatch_o <= 32'd0;
    end else begin
      if (wr_valid_i && wr_legal_c) begin
        mem_q[wr_addr_c] <= {wr_owner_gen_i, wr_pal_slot_i, wr_pal_gen_i,
                             wr_format_i, wr_frac_v_i, wr_frac_u_i,
                             wr_byte_sel_i, wr_nibble_i, 1'b0};
        writes_o <= writes_o + 32'd1;
      end

      // THE RECORD HOLDS AS A WHOLE, and the gate is the entire point.
      //
      // This assignment was unconditional. `rd_q` therefore tracked whatever
      // address was being OFFERED, every cycle, while the island's join stage
      // held its data and token across a dispatcher stall -- so a legal
      // sequence produced response A's data, response A's token and response
      // B's METADATA, with every accepted/emitted counter balancing perfectly.
      // Reproduced at the seam in `tests/texture/metajoin_seam_directed.cpp`
      // and written up in reports/D0-JOIN-SEAM-REPRODUCED-20260908.md.
      //
      // `rd_valid_i` is driven by `cache_smp_valid && r1_room_c`, which is the
      // SAME expression that enables the join's `r1_d_q`/`r1_t_q`. Gating on it
      // makes the bank and the join load on identical cycles: the metadata now
      // belongs to the data beside it by construction rather than by timing.
      //
      // It also un-blinds the generation check below. `rd_gen_q` moved with the
      // offered address too, so both operands of that comparison were corrupted
      // in lockstep and it could never fire on this defect -- a live identity
      // counter sitting next to the bug, reading zero. Inside the gate the
      // captured generation belongs to the same read as the row, so a genuine
      // staleness is visible again.
      if (rd_valid_i && rd_legal_c) begin
        rd_q     <= mem_q[rd_addr_c];
        rd_gen_q <= rd_owner_gen_i;
      end
      rd_v_q   <= rd_valid_i && rd_legal_c;

      if (rd_valid_i && rd_legal_c)  reads_o <= reads_o + 32'd1;
      if (rd_valid_i && !rd_legal_c) rd_illegal_sidx_o <= rd_illegal_sidx_o + 32'd1;

      if (rd_v_q && (rd_q[OGEN_LO +: GENW] != rd_gen_q))
        rd_gen_mismatch_o <= rd_gen_mismatch_o + 32'd1;
    end
  end

  assign rd_result_valid_o = rd_v_q;
  assign rd_pal_slot_o     = rd_q[PSLOT_LO +: PALSW];
  assign rd_pal_gen_o      = rd_q[PGEN_LO  +: GENW];
  assign rd_format_o       = rd_q[FMT_LO   +: 3];
  assign rd_frac_v_o       = rd_q[FRACV_LO +: 8];
  assign rd_frac_u_o       = rd_q[FRACU_LO +: 8];
  assign rd_byte_sel_o     = rd_q[BSEL_LO];
  assign rd_nibble_o       = rd_q[NIB_LO];

  // The STORED generation, not the offered one. `rd_gen_q` is what the row
  // carried when it was read; a consumer comparing it against the generation in
  // its own token can see a stale join without trusting this block to have
  // noticed. That is the difference between an instrument and a contract.
  assign rd_owner_gen_o    = rd_gen_q;

endmodule
