// zhao_texture_uv_join_v2.sv -- Packet-B descriptor / perspective-UV join.
//
// The two inputs are independent ready/valid records:
//
//   desc_data_i = {owner14, logical287}        (301 bits)
//   uv_data_i   = {owner14, U32, V32}          (78 bits)
//
// The one output record is exactly:
//
//   out_data_o  = {owner14, logical287, U32, V32} (365 bits)
//
// There is NO page-generation sidecar.  The sole captured binding-page
// generation is logical287[286:279], hence out_data_o[350:343].  It is part of
// the descriptor payload captured at the descriptor handshake and is never
// reread from live configuration.
//
// Each input owns one held register.  The output owns one held register.  A
// matching pair moves only when the output register has reserved capacity, and
// all 365 output bits then hold together through backpressure.  Same-edge
// output retirement, pair transfer, and input replacement are supported, so an
// already-filled stream acquires no join bubble.
`default_nettype none

module zhao_texture_uv_join_v2 (
    input var logic clk,
    input var logic rst_n,

    // Descriptor input: {owner14, logical287}.
    input  var logic         desc_valid_i,
    output var logic         desc_ready_o,
    input  var logic [300:0] desc_data_i,

    // Perspective result input: {owner14, U32, V32}.
    input  var logic        uv_valid_i,
    output var logic        uv_ready_o,
    input  var logic [77:0] uv_data_i,

    // Joined output: {owner14, logical287, U32, V32}.
    output var logic         out_valid_o,
    input  var logic         out_ready_i,
    output var logic [364:0] out_data_o,

    // A mismatch is internal corruption, not a recovery input.  The pair is
    // discarded, no joined work is emitted, the modulo-2^32 detector advances
    // exactly once, and the lifetime fault remains set until reset.  The outer
    // owner/reset barrier, never an ordinary frame clear, recovers obligations.
    output var logic [31:0] uvjoin_owner_mismatch_o,
    output var logic        lifetime_fault_o,

    // True only when both input holds and the output hold are empty.
    output var logic        idle_o
);

  localparam int unsigned OWNER_W       = 14;
  localparam int unsigned LOGICAL_W     = 287;
  localparam int unsigned UV_COMPONENT_W= 32;
  localparam int unsigned DESC_RECORD_W = OWNER_W + LOGICAL_W;
  localparam int unsigned UV_RECORD_W   = OWNER_W + 2*UV_COMPONENT_W;
  localparam int unsigned JOINED_W      = OWNER_W + LOGICAL_W + 2*UV_COMPONENT_W;

  localparam int unsigned DESC_OWNER_LO = LOGICAL_W;
  localparam int unsigned UV_OWNER_LO   = 2*UV_COMPONENT_W;
  localparam int unsigned UV_U_LO       = UV_COMPONENT_W;
  localparam int unsigned UV_V_LO       = 0;

  // In the joined record logical bit zero starts above V and U.  The sole page
  // generation is therefore [64+286 : 64+279] = [350:343].
  localparam int unsigned JOIN_LOGICAL_LO = 2*UV_COMPONENT_W;
  localparam int unsigned PAGE_GEN_LO     = JOIN_LOGICAL_LO + 279;
  localparam int unsigned PAGE_GEN_HI     = JOIN_LOGICAL_LO + 286;

  initial begin : p_layout_contract
    if ((DESC_RECORD_W != 301) || (UV_RECORD_W != 78) || (JOINED_W != 365))
      $fatal(1, "zhao_texture_uv_join_v2: record width contract changed");
    if ((PAGE_GEN_LO != 343) || (PAGE_GEN_HI != 350))
      $fatal(1, "zhao_texture_uv_join_v2: sole page-generation slice moved");
  end

  logic         desc_v_q;
  logic [300:0] desc_q;
  logic         uv_v_q;
  logic [77:0]  uv_q;
  logic         out_v_q;
  logic [364:0] out_q;

  wire [OWNER_W-1:0] desc_owner_c =
      desc_q[DESC_OWNER_LO +: OWNER_W];
  wire [OWNER_W-1:0] uv_owner_c =
      uv_q[UV_OWNER_LO +: OWNER_W];

  wire pair_present_c = desc_v_q && uv_v_q;
  wire owners_match_c = (desc_owner_c == uv_owner_c);
  wire out_room_c      = !out_v_q || out_ready_i;

  // A good pair consumes both input holds only after reserving the output. A bad
  // pair is discarded without acknowledging any same-edge replacement, then the
  // reset-lifetime barrier closes both input handshakes and all future joins.
  // This prevents apparently valid work from escaping after owner identity has
  // become structurally untrustworthy.
  wire fault_free_c = !lifetime_fault_o;
  wire join_c = fault_free_c && pair_present_c && owners_match_c && out_room_c;
  wire mismatch_c = fault_free_c && pair_present_c && !owners_match_c;

  assign desc_ready_o = fault_free_c && (!desc_v_q || join_c);
  assign uv_ready_o   = fault_free_c && (!uv_v_q   || join_c);

  assign out_valid_o = out_v_q;
  assign out_data_o  = out_q;
  assign idle_o      = !desc_v_q && !uv_v_q && !out_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      desc_v_q                  <= 1'b0;
      uv_v_q                    <= 1'b0;
      out_v_q                   <= 1'b0;
      uvjoin_owner_mismatch_o   <= 32'd0;
      lifetime_fault_o          <= 1'b0;
    end else begin
      // A mismatched held pair is discarded without accepting the currently
      // offered records. Otherwise each producer advances only when its own held
      // record has room. Once lifetime_fault_o rises, both ready outputs remain
      // low until reset and these holds stay empty.
      if (mismatch_c) begin
        desc_v_q <= 1'b0;
        uv_v_q   <= 1'b0;
      end else begin
        if (desc_ready_o) begin
          desc_v_q <= desc_valid_i;
          if (desc_valid_i) desc_q <= desc_data_i;
        end
        if (uv_ready_o) begin
          uv_v_q <= uv_valid_i;
          if (uv_valid_i) uv_q <= uv_data_i;
        end
      end

      // The complete 365-bit record is written once.  While out_v_q is stalled,
      // out_room_c is false and neither this payload nor its valid can change.
      if (out_room_c) begin
        out_v_q <= join_c;
        if (join_c) begin
          out_q <= {
            desc_owner_c,
            desc_q[LOGICAL_W-1:0],
            uv_q[UV_U_LO +: UV_COMPONENT_W],
            uv_q[UV_V_LO +: UV_COMPONENT_W]
          };
        end
      end

      // The lifetime fault has no frame-clear path.  A mismatch means admitted
      // owner obligations may now be stranded outside this join; only rst_n may
      // clear the structural-fault state and its counter.
      if (mismatch_c) begin
        uvjoin_owner_mismatch_o <= uvjoin_owner_mismatch_o + 32'd1;
        lifetime_fault_o        <= 1'b1;
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  logic [364:0] stalled_out_q;
  logic stalled_out_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      armed_q         <= 1'b0;
      stalled_out_q   <= '0;
      stalled_out_v_q <= 1'b0;
    end else begin
      armed_q <= 1'b1;
      if (out_valid_o && !out_ready_i) begin
        if (stalled_out_v_q)
          a_output_hold : assert (out_data_o == stalled_out_q);
        stalled_out_q   <= out_data_o;
        stalled_out_v_q <= 1'b1;
      end else begin
        stalled_out_v_q <= 1'b0;
      end

      if (armed_q) begin
        a_no_output_on_mismatch : assert (!(mismatch_c && join_c));
        a_join_owner_match : assert (!join_c || (desc_owner_c == uv_owner_c));
        a_mismatch_no_replacement : assert (!mismatch_c
            || (!desc_ready_o && !uv_ready_o));
        a_lifetime_barrier : assert (!lifetime_fault_o
            || (!desc_ready_o && !uv_ready_o && !join_c && !mismatch_c));
      end
    end
  end
`endif

endmodule : zhao_texture_uv_join_v2

`default_nettype wire
