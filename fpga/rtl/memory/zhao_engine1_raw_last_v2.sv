// zhao_engine1_raw_last_v2.sv -- Packet-H ENGINE1 raw16 retirement framer.
//
// One accepted ENGINE1 guard request is followed by one guard verdict and then
// exactly len_bytes/2 raw halfwords.  Completion is NOT inferred from the raw
// stream: controller_retire_halfwords_i is the independent physical retirement
// fact and is shaped for a direct client_rsp[3].credits connection.
//
// Nonterminal halfwords pass through while the downstream is ready.  The final
// raw halfword is withheld until controller retirement proves the exact request
// count, then is held as a conventional valid/data/LAST tuple until ready.  This
// STREAM -> FINAL_HELD split makes a real missing final raw pulse observable at
// physical retirement instead of turning silence into an indefinitely armed
// transaction.
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

  // ENGINE1-qualified raw return.  Each asserted pulse is one halfword.  The
  // data input is needed because the terminal halfword can be held for framing.
  input  logic       raw16_valid_i,
  input  logic [15:0] raw16_data_i,

  // Independent controller retirement, in 16-bit halfwords.  This is exactly
  // the shape and unit of zhao_arb_rsp_t.credits/client_rsp[3].credits.  Legal
  // controller bursts return 1..8; zero means no retirement this cycle.
  input  logic [7:0] controller_retire_halfwords_i,

  // Framed raw stream.  Nonterminal raw pulses cannot be backpressured at the
  // SDRAM edge, so ready must be high for them; a low ready on such a pulse is
  // a structural fault.  The independently validated terminal tuple is held
  // stable until this ready is high.
  input  logic        framed_raw_ready_i,
  output logic        framed_raw_valid_o,
  output logic [15:0] framed_raw_data_o,
  output logic        framed_raw_last_o,

  // 0 = idle, 1 = waiting for the verdict, 2 = approved/armed.  FINAL_HELD
  // deliberately retains public state 2 and armed_o=1 until its output handshake.
  output logic [1:0] state_o,
  output logic       idle_o,
  output logic       pending_verdict_o,
  output logic       armed_o,
  output logic [5:0] expected_halfwords_o,
  output logic [5:0] retired_halfwords_o,

  // Legacy alias retained for existing users; it now describes the framed
  // output tuple, including hold, rather than the unregistered raw input cycle.
  output logic       raw_last_o,
  output logic       structural_fault_o
);

  localparam logic [1:0] S_IDLE     = 2'd0;
  localparam logic [1:0] S_PENDING  = 2'd1;
  localparam logic [1:0] S_ARMED    = 2'd2;

  logic [1:0]  state_q;
  logic [5:0]  expected_halfwords_q;
  logic [5:0]  retired_halfwords_q;
  logic [5:0]  controller_retired_halfwords_q;
  logic        length_legal_q;
  logic        final_candidate_q;
  logic [15:0] final_data_q;
  logic        final_held_q;

  logic [6:0] retired_plus_one_c;
  logic [6:0] raw_after_cycle_c;
  logic [8:0] controller_retired_next_c;
  logic       accepted_length_legal_c;
  logic       legal_length_c;
  logic       stream_active_c;
  logic       exact_raw_terminal_c;
  logic       controller_retire_c;
  logic       controller_terminal_c;
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
  logic       controller_retire_wrong_state_c;
  logic       controller_retire_shape_c;
  logic       controller_retire_excess_c;
  logic       controller_raw_count_mismatch_c;
  logic       terminal_raw_mismatch_c;
  logic       raw_count_excess_c;
  logic       duplicate_final_raw_c;
  logic       terminal_action_mismatch_c;
  logic       framed_backpressure_c;
  logic       published_last_mismatch_c;
  logic       protocol_event_c;

  assign state_o              = state_q;
  assign idle_o               = (state_q == S_IDLE);
  assign pending_verdict_o    = (state_q == S_PENDING);
  assign armed_o              = (state_q == S_ARMED);
  assign expected_halfwords_o = expected_halfwords_q;
  // Preserve the established observation: this is the accepted raw-halfword
  // count, while controller_retired_halfwords_q is the independent validator.
  assign retired_halfwords_o  = retired_halfwords_q;

  assign stream_active_c = (state_q == S_ARMED) && !final_held_q;

  // Seven bits keep the comparison honest at the terminal edge without adding
  // a raw counter: the stored raw count remains exactly six bits.
  assign retired_plus_one_c = {1'b0, retired_halfwords_q} + 7'd1;
  assign raw_after_cycle_c = {1'b0, retired_halfwords_q}
                           + (raw16_valid_i ? 7'd1 : 7'd0);
  assign controller_retired_next_c =
      {3'b000, controller_retired_halfwords_q}
      + {1'b0, controller_retire_halfwords_i};

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

  // terminal_c is the mutant-selectable action view.  The exact raw and
  // controller comparisons below are independent detector operands: mutating an
  // action cannot move both sides of its checker together.
  assign terminal_c = `ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR(
                        retired_plus_one_c,
                        {1'b0, expected_halfwords_q});
  assign exact_raw_terminal_c = raw16_valid_i
                             && (retired_plus_one_c
                                 == {1'd0, expected_halfwords_q});

  assign controller_retire_c = (controller_retire_halfwords_i != 8'd0);
  assign controller_terminal_c = controller_retire_c
                              && (controller_retire_halfwords_i <= 8'd8)
                              && (controller_retired_next_c
                                  == {3'b000, expected_halfwords_q});

  // STREAM tuples are direct and nonterminal.  The exact terminal raw word is
  // captured instead; after independent controller validation FINAL_HELD owns
  // the output until ready.  No terminal marker is synthesized from silence.
  always_comb begin
    framed_raw_valid_o = 1'b0;
    framed_raw_data_o  = final_data_q;
    if (!structural_fault_o) begin
      if (stream_active_c && raw16_valid_i
          && (retired_plus_one_c < {1'b0, expected_halfwords_q})) begin
        framed_raw_valid_o = 1'b1;
        framed_raw_data_o  = raw16_data_i;
      end else if ((state_q == S_ARMED) && final_held_q) begin
        framed_raw_valid_o = 1'b1;
        framed_raw_data_o  = final_data_q;
      end
    end
  end

  assign raw_last_c = `ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR(
                        framed_raw_valid_o, (final_held_q || terminal_c),
                        retired_plus_one_c,
                        {1'b0, expected_halfwords_q});
  assign framed_raw_last_o = raw_last_c && !structural_fault_o;
  assign raw_last_o        = framed_raw_last_o;

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

  // Raw or controller evidence belongs only to STREAM.  FINAL_HELD has already
  // retired the physical request and accepts only the framed output handshake.
  assign raw_wrong_state_c = raw16_valid_i && !stream_active_c;
  assign controller_retire_wrong_state_c = controller_retire_c
                                         && !stream_active_c;
  assign controller_retire_shape_c = controller_retire_c
                                   && (controller_retire_halfwords_i > 8'd8);
  assign controller_retire_excess_c = controller_retire_c
                                    && !controller_retire_shape_c
                                    && (controller_retired_next_c
                                        > {3'b000, expected_halfwords_q});

  // At each nonterminal physical burst retirement, the cumulative controller
  // count must equal the number of raw halfwords actually seen through that
  // cycle.  The request-terminal boundary owns one exact predicate covering a
  // missing pulse, a short stream, or an absent/mismatched final candidate.
  assign controller_raw_count_mismatch_c = stream_active_c
                                         && controller_retire_c
                                         && !controller_retire_shape_c
                                         && !controller_retire_excess_c
                                         && !controller_terminal_c
                                         && (controller_retired_next_c
                                             != {2'b00, raw_after_cycle_c});
  assign terminal_raw_mismatch_c = stream_active_c && controller_terminal_c
                                 && ((raw_after_cycle_c
                                      != {1'd0, expected_halfwords_q})
                                     || !(final_candidate_q
                                          || exact_raw_terminal_c));
  assign raw_count_excess_c = stream_active_c && raw16_valid_i
                            && (retired_plus_one_c
                                > {1'd0, expected_halfwords_q});
  assign duplicate_final_raw_c = stream_active_c && final_candidate_q
                               && raw16_valid_i;

  // Mutant action checks use exact independently computed operands.  They fire
  // on the cycle of the altered behaviour, not by comparing two values clocked
  // by the same faulty enable.
  assign terminal_action_mismatch_c = stream_active_c && raw16_valid_i
                                    && (terminal_c
                                        != exact_raw_terminal_c);
  assign framed_backpressure_c = stream_active_c && framed_raw_valid_o
                               && !framed_raw_ready_i;
  assign published_last_mismatch_c = framed_raw_valid_o
                                   && (framed_raw_last_o
                                       != final_held_q);

  assign protocol_event_c = state_invalid_c
                         || accept_overlap_c
                         || verdict_without_pending_c
                         || verdict_both_c
                         || illegal_approval_c
                         || controller_retire_wrong_state_c
                         || controller_retire_shape_c
                         || controller_retire_excess_c
                         || controller_raw_count_mismatch_c
                         || terminal_raw_mismatch_c
                         || raw_count_excess_c
                         || duplicate_final_raw_c
                         || terminal_action_mismatch_c
                         || framed_backpressure_c
                         || published_last_mismatch_c
                         || raw_wrong_state_c;

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
      state_q                            <= S_IDLE;
      expected_halfwords_q               <= 6'd0;
      retired_halfwords_q                <= 6'd0;
      controller_retired_halfwords_q     <= 6'd0;
      length_legal_q                     <= 1'b0;
      final_candidate_q                  <= 1'b0;
      final_data_q                       <= 16'd0;
      final_held_q                       <= 1'b0;
      structural_fault_o                 <= 1'b0;
    end else if (structural_fault_o) begin
      // Reset-lifetime evidence is fail-stop.  Keep the externally visible
      // state legal and disarmed until reset; no later pulse can be accepted.
      state_q                            <= S_IDLE;
      expected_halfwords_q               <= 6'd0;
      retired_halfwords_q                <= 6'd0;
      controller_retired_halfwords_q     <= 6'd0;
      length_legal_q                     <= 1'b0;
      final_candidate_q                  <= 1'b0;
      final_data_q                       <= 16'd0;
      final_held_q                       <= 1'b0;
    end else if (protocol_event_c) begin
      structural_fault_o                 <= 1'b1;
      state_q                            <= S_IDLE;
      expected_halfwords_q               <= 6'd0;
      retired_halfwords_q                <= 6'd0;
      controller_retired_halfwords_q     <= 6'd0;
      length_legal_q                     <= 1'b0;
      final_candidate_q                  <= 1'b0;
      final_data_q                       <= 16'd0;
      final_held_q                       <= 1'b0;
    end else begin
      case (state_q)
        S_IDLE: begin
          if (guard_accept_i) begin
            state_q                        <= S_PENDING;
            expected_halfwords_q           <= len_bytes_i[6:1];
            retired_halfwords_q            <= 6'd0;
            controller_retired_halfwords_q <= 6'd0;
            length_legal_q                 <= accepted_length_legal_c;
            final_candidate_q              <= 1'b0;
            final_data_q                   <= 16'd0;
            final_held_q                   <= 1'b0;
          end
        end

        S_PENDING: begin
          if (verdict_denied_i) begin
            // A refusal is terminal and never retires a raw halfword.
            state_q                        <= S_IDLE;
            expected_halfwords_q           <= 6'd0;
            retired_halfwords_q            <= 6'd0;
            controller_retired_halfwords_q <= 6'd0;
            length_legal_q                 <= 1'b0;
            final_candidate_q              <= 1'b0;
            final_data_q                   <= 16'd0;
            final_held_q                   <= 1'b0;
          end else if (verdict_ok_i) begin
            // Illegal approval was consumed by protocol_event_c above.  This
            // branch therefore arms only one of the three legal lengths.
            state_q                        <= S_ARMED;
            retired_halfwords_q            <= 6'd0;
            controller_retired_halfwords_q <= 6'd0;
            final_candidate_q              <= 1'b0;
            final_data_q                   <= 16'd0;
            final_held_q                   <= 1'b0;
          end
        end

        S_ARMED: begin
          if (final_held_q) begin
            if (framed_raw_ready_i) begin
              // The validated terminal tuple was physically consumed.  Do not
              // let yesterday's legality, data, or counts colour the next request.
              state_q                        <= S_IDLE;
              expected_halfwords_q           <= 6'd0;
              retired_halfwords_q            <= 6'd0;
              controller_retired_halfwords_q <= 6'd0;
              length_legal_q                 <= 1'b0;
              final_candidate_q              <= 1'b0;
              final_data_q                   <= 16'd0;
              final_held_q                   <= 1'b0;
            end
          end else begin
            if (raw16_valid_i) begin
              retired_halfwords_q <= retired_plus_one_c[5:0];
              if (exact_raw_terminal_c) begin
                final_candidate_q <= 1'b1;
                final_data_q      <= raw16_data_i;
              end
            end

            if (controller_retire_c)
              controller_retired_halfwords_q
                  <= controller_retired_next_c[5:0];

            if (controller_terminal_c) begin
              // protocol_event_c proved exact count alignment and the presence
              // of the final candidate before this state transition can occur.
              final_held_q <= 1'b1;
              if (exact_raw_terminal_c)
                final_data_q <= raw16_data_i;
            end
          end
        end

        default: begin
          // state_invalid_c normally catches this before the edge; retain a
          // deterministic fail-stop fallback for synthesis/simulation parity.
          structural_fault_o                 <= 1'b1;
          state_q                            <= S_IDLE;
          expected_halfwords_q               <= 6'd0;
          retired_halfwords_q                <= 6'd0;
          controller_retired_halfwords_q     <= 6'd0;
          final_candidate_q                  <= 1'b0;
          final_data_q                       <= 16'd0;
          final_held_q                       <= 1'b0;
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
