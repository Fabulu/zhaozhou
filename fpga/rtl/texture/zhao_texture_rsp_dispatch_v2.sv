// zhao_texture_rsp_dispatch_v2.sv — Packet-B terminal response collector.
//
// Placement is AFTER CLUT palette resolution, direct-nearest decode, bilinear
// assembly, and local error construction.  Every input is therefore one final,
// immutable record at the selected parameters:
//
//   {route_token18, status8, raw_index8, alpha8, RGB24}
//
// Four independent one-entry holds reserve simultaneous class completions.  A
// retained round-robin cursor fairly selects them into one separately held owner
// return.  Every tuple bit is copied exactly: this block never drops or
// reconstructs raw index, narrows status, relabels a token, or edits loud-error
// colour.  Token class is checked against the physical input channel on the
// acceptance edge; disagreement is counted while the original tuple continues
// unchanged so the detector and the evidence cannot cancel in lockstep.
//
// Raw cache steering is deliberately not here.  The cache holds its complete raw
// record while top-level ready/valid routing feeds the class processors.  The
// unversioned raw-cache demultiplexer remains the old-island oracle.
//
// ENFORCED-BY: tests/texture/texture_rsp_dispatch_v2_directed.cpp
// POSITIVE-CONTROLS: tests/mutants/zhao_texture_rsp_dispatch_v2_index_mutants.sv
`default_nettype none

module zhao_texture_rsp_dispatch_v2 #(
    parameter int unsigned ROUTEW = 18
) (
    input var logic clk,
    input var logic rst_n,

    // ---- final class completions --------------------------------------------
    input  var logic clut_valid_i,
    output var logic clut_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                            clut_tuple_i,

    input  var logic near_valid_i,
    output var logic near_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                            near_tuple_i,

    input  var logic bil_valid_i,
    output var logic bil_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                            bil_tuple_i,

    input  var logic err_valid_i,
    output var logic err_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                            err_tuple_i,

    // ---- one held owner return ---------------------------------------------
    output var logic out_valid_o,
    input  var logic out_ready_i,
    output var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                            out_tuple_o,

    // Fixed {ERR,BIL,NEAR,CLUT} observation order.  Each bit is the physical
    // one-entry reservation belonging to that class.
    output var logic [3:0] pending_valid_o,
    output var logic       idle_o,

    // accepted_o may advance by 0..4 per clock; emitted_o by 0..1.
    output var logic [31:0] accepted_o,
    output var logic [31:0] emitted_o,
    output var logic [31:0] class_mismatch_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned TUPLEW = ROUTEW + TEXTURE_RESULT_W;
  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;
  localparam logic [1:0] CLS_ERR  = 2'd3;

  initial begin : p_packet_b_shape
    if (ROUTEW != 18)
      $fatal(1, "rsp_dispatch_v2: ROUTEW must be the Packet-B route-token width 18");
    if (TUPLEW != 66)
      $fatal(1, "rsp_dispatch_v2: terminal tuple must be exactly 66 bits");
  end

  // Channel index is the route-class encoding, so pending_valid_o naturally has
  // the required bit order: [3]=ERR, [2]=BIL, [1]=NEAR, [0]=CLUT.
  logic [3:0]        slot_valid_q;
  logic [TUPLEW-1:0] slot_tuple_q [0:3];
  logic [1:0]        rr_q;

  logic              out_valid_q;
  logic [TUPLEW-1:0] out_tuple_q;

  logic [3:0]         in_valid_c;
  logic [3:0]         in_ready_c;
  logic [TUPLEW-1:0] in_tuple_c [0:3];
  logic [3:0]         push_c;
  logic [3:0]         pop_c;

  logic       out_room_c;
  logic       grant_valid_c;
  logic [1:0] grant_index_c;
  logic       load_output_c;
  logic [2:0] accept_count_c;
  logic [2:0] mismatch_count_c;

  always_comb begin
    in_valid_c[CLS_CLUT] = clut_valid_i;
    in_valid_c[CLS_NEAR] = near_valid_i;
    in_valid_c[CLS_BIL]  = bil_valid_i;
    in_valid_c[CLS_ERR]  = err_valid_i;
    in_tuple_c[CLS_CLUT] = clut_tuple_i;
    in_tuple_c[CLS_NEAR] = near_tuple_i;
    in_tuple_c[CLS_BIL]  = bil_tuple_i;
    in_tuple_c[CLS_ERR]  = err_tuple_i;

    out_room_c = !out_valid_q || out_ready_i;

    // Retained round-robin priority.  Only held reservations participate; live
    // input valid cannot create a combinational input-to-output valid path.
    grant_valid_c = 1'b0;
    grant_index_c = rr_q;
    for (int unsigned offset = 0; offset < 4; offset++) begin
      automatic logic [1:0] candidate = rr_q + 2'(offset);
      if (!grant_valid_c && slot_valid_q[candidate]) begin
        grant_valid_c = 1'b1;
        grant_index_c = candidate;
      end
    end

    load_output_c = out_room_c && grant_valid_c;
    pop_c = 4'b0000;
    if (load_output_c)
      pop_c[grant_index_c] = 1'b1;

    // A selected slot returns its credit on the same edge it moves to the output,
    // allowing a replacement completion without a needless bubble.
    in_ready_c = ~slot_valid_q | pop_c;
    push_c = in_valid_c & in_ready_c;

    clut_ready_o = in_ready_c[CLS_CLUT];
    near_ready_o = in_ready_c[CLS_NEAR];
    bil_ready_o  = in_ready_c[CLS_BIL];
    err_ready_o  = in_ready_c[CLS_ERR];

    accept_count_c = 3'd0;
    mismatch_count_c = 3'd0;
    for (int unsigned channel = 0; channel < 4; channel++) begin
      accept_count_c = accept_count_c + 3'(push_c[channel]);
      if (push_c[channel] &&
          (in_tuple_c[channel][TUPLEW-1 -: 2] != 2'(channel)))
        mismatch_count_c = mismatch_count_c + 3'd1;
    end
  end

  assign out_valid_o     = out_valid_q;
  assign out_tuple_o     = out_tuple_q;
  assign pending_valid_o = slot_valid_q;
  assign idle_o          = !out_valid_q && !(|slot_valid_q);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      slot_valid_q   <= 4'b0000;
      rr_q           <= CLS_CLUT;
      out_valid_q    <= 1'b0;
      accepted_o     <= 32'd0;
      emitted_o      <= 32'd0;
      class_mismatch_o <= 32'd0;
    end else begin
      // Four independent one-entry holds.  Push wins over a simultaneous pop so
      // a selected class can replace its reservation on the same edge.
      for (int unsigned channel = 0; channel < 4; channel++) begin
        if (push_c[channel]) begin
          slot_valid_q[channel] <= 1'b1;
          slot_tuple_q[channel] <= in_tuple_c[channel];
        end else if (pop_c[channel]) begin
          slot_valid_q[channel] <= 1'b0;
        end
      end

      // The output register alone owns the external hold law.
      if (out_room_c) begin
        out_valid_q <= grant_valid_c;
        if (grant_valid_c)
          out_tuple_q <= slot_tuple_q[grant_index_c];
      end

      if (load_output_c)
        rr_q <= grant_index_c + 2'd1;

      if (accept_count_c != 3'd0)
        accepted_o <= accepted_o + 32'(accept_count_c);
      if (out_valid_q && out_ready_i)
        emitted_o <= emitted_o + 32'd1;
      if (mismatch_count_c != 3'd0)
        class_mismatch_o <= class_mismatch_o + 32'(mismatch_count_c);
    end
  end

`ifndef SYNTHESIS
  logic assert_armed_q;
  logic [TUPLEW-1:0] held_out_q;
  logic held_out_valid_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      assert_armed_q    <= 1'b0;
      held_out_valid_q  <= 1'b0;
    end else begin
      assert_armed_q <= 1'b1;
      held_out_valid_q <= out_valid_o && !out_ready_i;
      if (out_valid_o && !out_ready_i)
        held_out_q <= out_tuple_o;
    end
  end

  always_ff @(posedge clk) begin
    if (assert_armed_q && held_out_valid_q) begin
      a_output_holds:
        assert (out_valid_o && (out_tuple_o == held_out_q))
        else $error("rsp_dispatch_v2: terminal tuple changed while stalled");
    end
  end
`endif

endmodule : zhao_texture_rsp_dispatch_v2

`default_nettype wire
