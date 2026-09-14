// zhao_texture_metajoin_v2.sv -- Packet-B observation-only successor to
// zhao_texture_metajoin.
//
// The 256x40 metadata image, synchronous read, generation comparison, counters,
// and every acceptance edge are unchanged.  idle_o observes the existing held
// synchronous read result only.  External read/write offers are separate named
// top-level channel terms; no offer is converted into state or a ready path here.
`default_nettype none

module zhao_texture_metajoin_v2 #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned SIDXW = 2,
    parameter int unsigned GENW  = 8,
    parameter int unsigned PALSW = 2,
    parameter int unsigned METAW = 40
) (
    input var logic clk,
    input var logic rst_n,

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
    output var logic [GENW-1:0]      rd_owner_gen_o,

    output var logic [31:0]          writes_o,
    output var logic [31:0]          reads_o,
    output var logic [31:0]          rd_illegal_sidx_o,
    output var logic [31:0]          rd_gen_mismatch_o,
    output var logic                 idle_o
);

  localparam int unsigned ROWS  = 1 << (SLOTW + SIDXW);
  localparam int unsigned ADDRW = SLOTW + SIDXW;

  localparam int unsigned RSVD_W   = 1;
  localparam int unsigned NIB_LO   = RSVD_W;
  localparam int unsigned BSEL_LO  = NIB_LO + 1;
  localparam int unsigned FRACU_LO = BSEL_LO + 1;
  localparam int unsigned FRACV_LO = FRACU_LO + 8;
  localparam int unsigned FMT_LO   = FRACV_LO + 8;
  localparam int unsigned PGEN_LO  = FMT_LO + 3;
  localparam int unsigned PSLOT_LO = PGEN_LO + GENW;
  localparam int unsigned OGEN_LO  = PSLOT_LO + PALSW;

  initial begin : p_layout_contract
    if (OGEN_LO + GENW != METAW)
      $fatal(1, "zhao_texture_metajoin_v2: field offsets do not fill METAW");
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

      if (rd_valid_i && rd_legal_c) begin
        rd_q     <= mem_q[rd_addr_c];
        rd_gen_q <= rd_owner_gen_i;
      end
      rd_v_q <= rd_valid_i && rd_legal_c;

      if (rd_valid_i && rd_legal_c)
        reads_o <= reads_o + 32'd1;
      if (rd_valid_i && !rd_legal_c)
        rd_illegal_sidx_o <= rd_illegal_sidx_o + 32'd1;

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
  assign rd_owner_gen_o    = rd_gen_q;

  // rd_v_q is the only retained transaction state in this leaf.  RAM payload is
  // deliberately unreset and is not busy merely because stale bits exist.
  assign idle_o = !rd_v_q;

endmodule : zhao_texture_metajoin_v2

`default_nettype wire
