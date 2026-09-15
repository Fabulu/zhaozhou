// zhao_engine1_raw_last_v2.sv -- Packet-H ENGINE1 raw16 LAST leaf.
//
// One accepted ENGINE1 guard request is followed by one guard verdict and then
// exactly len_bytes/2 raw halfword pulses.  The request acceptance and verdict
// are deliberately separate events: guard ready is not permission to retire a
// return.  The raw16 stream has no data path here; this leaf owns only its
// bounded retirement count and the physical LAST sideband.
//
// The source list places the committed mutant selector immediately before this
// file for inverse-polarity tests.  Ordinary builds define no selector.
`default_nettype none

`ifndef ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR
  `define ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR(next_count, expected_count) \
    ((next_count) == (expected_count))
`endif
`ifndef ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR
  `define ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR(raw_valid, terminal, next_count, expected_count) \
    ((raw_valid) && (terminal))
`endif

`ifndef ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR
  `define ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_value) (state_value)
`endif

module zhao_engine1_raw_last_v2
(
  input  logic       clk,
  input  logic       rst_n,

  // One physical ENGINE1 guard acceptance.  This is already the muxed
  // guard_req.valid && guard_rsp.ready event; no source-valid is re-created.
  input  logic       guard_accept_i,
  input  logic [6:0] len_bytes_i,

  // Registered guard verdict, delivered after the acceptance edge.
  input  logic       verdict_ok_i,
  input  logic       verdict_denied_i,

  // ENGINE1-qualified raw return.  Each asserted pulse is one halfword.
  input  logic       raw16_valid_i,

  // 0 = idle, 1 = waiting for the verdict, 2 = approved/armed.
  output logic [1:0] state_o,
  output logic       idle_o,
  output logic       pending_verdict_o,
  output logic       armed_o,
  output logic [5:0] expected_halfwords_o,
  output logic [5:0] retired_halfwords_o,
  output logic       raw_last_o,
  output logic       structural_fault_o
);

  localparam logic [1:0] S_IDLE     = 2'd0;
  localparam logic [1:0] S_PENDING  = 2'd1;
  localparam logic [1:0] S_ARMED    = 2'd2;

  logic [1:0] state_q;
  logic [5:0] expected_halfwords_q;
  logic [5:0] retired_halfwords_q;
  logic       length_legal_q;

  logic [6:0] retired_plus_one_c;
  logic       accepted_length_legal_c;
  logic       legal_length_c;
  logic       terminal_c;
  logic       raw_last_c;
  logic [1:0] checked_state_c;
  logic       state_invalid_c;
  logic       verdict_any_c;
  logic       verdict_both_c;
  logic       accept_overlap_c;
  logic       verdict_without_pending_c;
  logic       illegal_approval_c;
  logic       raw_wrong_state_c;
  logic       protocol_event_c;

  assign state_o              = state_q;
  assign idle_o               = (state_q == S_IDLE);
  assign pending_verdict_o    = (state_q == S_PENDING);
  assign armed_o              = (state_q == S_ARMED);
  assign expected_halfwords_o = expected_halfwords_q;
  assign retired_halfwords_o  = retired_halfwords_q;

  // Seven bits keep the comparison honest at the terminal edge without adding
  // a counter: the stored retirement count remains exactly six bits.
  assign retired_plus_one_c = {1'b0, retired_halfwords_q} + 7'd1;

  // The only legal byte lengths are 16, 32 and 64, hence 8, 16 and 32
  // halfwords.  `len_bytes_i[6:1]` is the required len >> 1 capture.  The
  // one-bit legality latch preserves the byte-level distinction between, for
  // example, 16 and 17 bytes without adding a second length counter.
  assign accepted_length_legal_c = (len_bytes_i == 7'd16)
                                || (len_bytes_i == 7'd32)
                                || (len_bytes_i == 7'd64);
  assign legal_length_c = length_legal_q
                       && ((expected_halfwords_q == 6'd8)
                           || (expected_halfwords_q == 6'd16)
                           || (expected_halfwords_q == 6'd32));

  assign terminal_c = `ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR(
                        retired_plus_one_c,
                        {1'b0, expected_halfwords_q});
  assign raw_last_c = `ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR(
                        raw16_valid_i, terminal_c,
                        retired_plus_one_c,
                        {1'b0, expected_halfwords_q});

  assign checked_state_c = `ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_q);
  assign state_invalid_c = (checked_state_c != S_IDLE)
                        && (checked_state_c != S_PENDING)
                        && (checked_state_c != S_ARMED);
  assign verdict_any_c = verdict_ok_i || verdict_denied_i;
  assign verdict_both_c = verdict_ok_i && verdict_denied_i;

  // An acceptance is physical only in IDLE.  A source disappearing before
  // that event is upstream of this leaf and therefore cannot create one here.
  assign accept_overlap_c = guard_accept_i && (state_q != S_IDLE);
  assign verdict_without_pending_c = verdict_any_c && (state_q != S_PENDING);
  assign illegal_approval_c = (state_q == S_PENDING)
                           && verdict_ok_i && !verdict_denied_i
                           && !legal_length_c;

  // LAST is generated from the independent registered retirement count, not
  // from a raw-sideband input.  Any raw beat outside APPROVED/ARMED is a
  // structural protocol fault, including a beat after the terminal edge.
  assign raw_wrong_state_c = raw16_valid_i && (state_q != S_ARMED);

  assign protocol_event_c = state_invalid_c
                         || accept_overlap_c
                         || verdict_without_pending_c
                         || verdict_both_c
                         || illegal_approval_c
                         || raw_wrong_state_c;

  assign raw_last_o = (state_q == S_ARMED) && raw16_valid_i
                   && raw_last_c && !structural_fault_o;

  // The selector collision is intentionally an elaboration failure.  It keeps
  // mutually-exclusive inverse modes from silently making one another's test
  // meaningless.
  initial begin : p_selector_contract
`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
    $fatal(1, "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION: define exactly one selector");
`endif
  end
`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE_SELECTOR
      u_engine1_raw_last_v2_selector_collision_compile_fail();
`endif

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q              <= S_IDLE;
      expected_halfwords_q <= 6'd0;
      retired_halfwords_q  <= 6'd0;
      length_legal_q       <= 1'b0;
      structural_fault_o   <= 1'b0;
    end else if (structural_fault_o) begin
      // Reset-lifetime evidence is fail-stop.  Keep the externally visible
      // state legal and disarmed until reset; no later pulse can be accepted.
      state_q              <= S_IDLE;
      expected_halfwords_q <= 6'd0;
      retired_halfwords_q  <= 6'd0;
      length_legal_q       <= 1'b0;
    end else if (protocol_event_c) begin
      structural_fault_o   <= 1'b1;
      state_q              <= S_IDLE;
      expected_halfwords_q <= 6'd0;
      retired_halfwords_q  <= 6'd0;
      length_legal_q       <= 1'b0;
    end else begin
      case (state_q)
        S_IDLE: begin
          if (guard_accept_i) begin
            state_q              <= S_PENDING;
            expected_halfwords_q <= len_bytes_i[6:1];
            retired_halfwords_q  <= 6'd0;
            length_legal_q       <= accepted_length_legal_c;
          end
        end

        S_PENDING: begin
          if (verdict_denied_i) begin
            // A refusal is terminal and never retires a raw halfword.
            state_q              <= S_IDLE;
            expected_halfwords_q <= 6'd0;
            retired_halfwords_q  <= 6'd0;
            length_legal_q       <= 1'b0;
          end else if (verdict_ok_i) begin
            // Illegal approval was consumed by protocol_event_c above.  This
            // branch therefore arms only one of the three legal lengths.
            state_q             <= S_ARMED;
            retired_halfwords_q <= 6'd0;
          end
        end

        S_ARMED: begin
          if (raw16_valid_i) begin
            retired_halfwords_q <= retired_plus_one_c[5:0];
            if (terminal_c) begin
              // A complete approved return is a clean terminal event.  Do not
              // let yesterday's legality or counts colour the next request.
              state_q              <= S_IDLE;
              expected_halfwords_q <= 6'd0;
              retired_halfwords_q  <= 6'd0;
              length_legal_q       <= 1'b0;
            end
          end
        end

        default: begin
          // state_invalid_c normally catches this before the edge; retain a
          // deterministic fail-stop fallback for synthesis/simulation parity.
          structural_fault_o   <= 1'b1;
          state_q              <= S_IDLE;
          expected_halfwords_q <= 6'd0;
          retired_halfwords_q  <= 6'd0;
        end
      endcase
    end
  end

endmodule

`undef ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR
`undef ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR
`undef ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR
`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  `undef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
`endif
`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
  `undef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
`endif
`default_nettype wire
