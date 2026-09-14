// zhao_render_asset_mux.sv -- excluded ENGINE1 local owner mux (Packet E3).
//
// Authority: reports/PACKET-E-GUARDED-ASSET-REFUSAL-ABI-20260914.md section 3.
// Status: excluded:not-yet-adopted.  This block is deliberately not connected
// to zhao_shell_top: that shell does not yet expose an independently-derived
// raw16 LAST, and silence is not evidence that a return completed.
//
// GEOMETRY and TEXTURE_FILL are local owners, never global client identities.
// Exactly one captured local request is translated to the existing ENGINE1
// guard client, accepted separately from its later verdict, and retained through
// the complete raw return.  Malformed local offers are not repaired or narrowed:
// their original shape is retained and CLIENT_NONE makes the real guard deny.
`default_nettype none

// A committed selector shim may override one expression immediately before this
// exact source.  Ordinary builds use only these production defaults.
`ifndef ZHAO_RENDER_ASSET_GUARD_VALID_EXPR
  `define ZHAO_RENDER_ASSET_GUARD_VALID_EXPR(is_offer, is_verdict) (is_offer)
`endif
`ifndef ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR
  `define ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR(owner_texture, raw_count) (owner_texture)
`endif
`ifndef ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR
  `define ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR(exact_count) (exact_count)
`endif
`ifndef ZHAO_RENDER_ASSET_DENY_PULSE_EXPR
  `define ZHAO_RENDER_ASSET_DENY_PULSE_EXPR(intended_pulse) (intended_pulse)
`endif

module zhao_render_asset_mux
  import zhao_pkg::*;
(
    input  logic            clk,
    input  logic            rst_n,
    input  logic            frame_fault_clear_i,

    // Already-muxed geometry request.  A lawful offer is an ENGINE1 read with
    // exact 32/64-byte full-mask shape; addr and BE are preserved downstream.
    input  zhao_guard_req_t geom_req_i,
    output zhao_guard_rsp_t geom_rsp_o,
    output logic            geom_beat_valid_o,
    output logic [63:0]     geom_beat_data_o,
    output logic            geom_beat_last_o,

    // One texture-cache line fill.  Address is intentionally 32 bits so an
    // out-of-map bit cannot disappear while entering the 27-bit guard ABI.
    input  logic            texture_fill_valid_i,
    output logic            texture_fill_ready_o,
    input  logic [31:0]     texture_fill_addr_i,
    output logic            texture_fill_data_valid_o,
    output logic [15:0]     texture_fill_data_o,
    output logic            texture_fill_refused_o,

    // Sole downstream guard request and its later verdict.
    output zhao_guard_req_t guard_req_o,
    input  zhao_guard_rsp_t guard_rsp_i,

    // Raw ENGINE1 return.  LAST is mandatory and must come from an independent
    // physical request-retirement fact in any future shell-v2 composition.
    input  logic            engine1_raw16_valid_i,
    input  logic [15:0]     engine1_raw16_data_i,
    input  logic            engine1_raw16_last_i,

    // Fault/quiet evidence.  protocol_fault is frame-clearable only at quiet;
    // structural_fault means an approved geometry record became unrecoverable
    // and is reset-only.
    output logic            quiet_o,
    output logic            protocol_fault_o,
    output logic            structural_fault_o,

    // Modulo-32-bit transaction evidence.
    output logic [31:0]     guard_accepted_o,
    output logic [31:0]     guard_ok_o,
    output logic [31:0]     guard_denied_o,
    output logic [31:0]     geometry_accepted_o,
    output logic [31:0]     geometry_refused_o,
    output logic [31:0]     texture_accepted_o,
    output logic [31:0]     texture_refused_o,
    output logic [31:0]     contention_o,
    output logic [31:0]     raw_halfwords_o,
    output logic [31:0]     protocol_faults_o
);

  typedef enum logic [2:0] {
    M_IDLE       = 3'd0,
    M_OFFER      = 3'd1,
    M_VERDICT    = 3'd2,
    M_DATA       = 3'd3,
    M_DRAIN_BAD  = 3'd4,
    M_STRUCTURAL = 3'd5
  } mux_state_e;

  mux_state_e st_q;

  // The two selector operands are deliberately independent: rr_last_texture_q
  // changes only on a real guard acceptance, while the live wants are sampled
  // only to select a new whole request in M_IDLE.
  logic rr_last_texture_q;
  logic owner_texture_q;
  logic capture_live_q;
  logic selected_shape_ok_q;
  logic offer_source_faulted_q;
  zhao_guard_req_t guard_req_q;
  zhao_guard_req_t source_geom_req_q;
  logic [31:0] source_texture_addr_q;
  logic [5:0] expected_halfwords_q;
  logic [5:0] return_halfwords_q;
  logic [1:0] pack_halfwords_q;
  logic [63:0] pack_data_q;

  function automatic logic [63:0] mask_of(input logic [6:0] len_b);
    mask_of = 64'd0;
    for (int unsigned b = 0; b < 64; b++) begin
      if (b < len_b) mask_of[b] = 1'b1;
    end
  endfunction

  logic geom_wants_c, texture_wants_c, pick_texture_c;
  logic geom_shape_ok_c, texture_shape_ok_c;
  zhao_guard_req_t picked_req_c;
  logic picked_shape_ok_c;
  logic [5:0] picked_halfwords_c;

  always_comb begin
    geom_wants_c    = geom_req_i.valid;
    texture_wants_c = texture_fill_valid_i;
    pick_texture_c  = texture_wants_c;
    if (geom_wants_c && texture_wants_c)
      pick_texture_c = !rr_last_texture_q;
    else if (geom_wants_c)
      pick_texture_c = 1'b0;

    geom_shape_ok_c = !geom_req_i.write
                   && (geom_req_i.client == ZHAO_CLIENT_ENGINE1)
                   && ((geom_req_i.len == 7'd32)
                       || (geom_req_i.len == 7'd64))
                   && (geom_req_i.be == mask_of(geom_req_i.len));
    texture_shape_ok_c = (texture_fill_addr_i[31:27] == 5'd0)
                      && (texture_fill_addr_i[3:0] == 4'd0);

    picked_req_c       = '0;
    picked_shape_ok_c  = 1'b0;
    picked_halfwords_c = 6'd0;
    if (pick_texture_c) begin
      picked_req_c.valid  = 1'b1;
      picked_req_c.write  = 1'b0;
      picked_req_c.client = texture_shape_ok_c
                          ? ZHAO_CLIENT_ENGINE1 : ZHAO_CLIENT_NONE;
      picked_req_c.addr   = texture_fill_addr_i[26:0];
      picked_req_c.len    = 7'd16;
      picked_req_c.be     = 64'h0000_0000_0000_FFFF;
      picked_shape_ok_c   = texture_shape_ok_c;
      picked_halfwords_c  = 6'd8;
    end else begin
      // Preserve every geometry shape field except the two trusted substitutions.
      picked_req_c        = geom_req_i;
      picked_req_c.valid  = 1'b1;
      picked_req_c.write  = 1'b0;
      picked_req_c.client = geom_shape_ok_c
                          ? ZHAO_CLIENT_ENGINE1 : ZHAO_CLIENT_NONE;
      picked_shape_ok_c   = geom_shape_ok_c;
      picked_halfwords_c  = geom_req_i.len[6:1];
    end
  end

  // ---------------------------------------------------------------- request --
  // A held register, never a mux of live source payloads, drives the guard.
  // The normal expression is true in M_OFFER only. A selected source still owns
  // ready/valid until guard acceptance: disappearance or payload drift suppresses
  // VALID combinationally on that same cycle, so a newly-ready guard cannot take
  // the stale capture before the detecting edge cancels it. The committed replay
  // mutant extends the otherwise-valid offer through M_VERDICT so denial's
  // still-high ready causes a duplicate physical acceptance.
  logic guard_valid_c;
  logic guard_accept_c;
  always_comb begin
    guard_req_o       = guard_req_q;
    guard_valid_c     = `ZHAO_RENDER_ASSET_GUARD_VALID_EXPR(
                          (st_q == M_OFFER), (st_q == M_VERDICT))
                      && !((st_q == M_OFFER)
                           && (selected_source_missing_c
                               || selected_source_changed_c));
    guard_req_o.valid = guard_valid_c;
    guard_accept_c    = guard_req_o.valid && guard_rsp_i.ready;
  end

  // Source ready is exactly the captured guard acceptance, and only the selected
  // local owner sees it.  No valid expression depends on its own ready.
  always_comb begin
    geom_rsp_o              = '0;
    texture_fill_ready_o    = 1'b0;
    if (guard_accept_c && !owner_texture_q)
      geom_rsp_o.ready = 1'b1;
    if (guard_accept_c && owner_texture_q)
      texture_fill_ready_o = 1'b1;

    // Violation wins if a broken guard presents both bits.  This creates one
    // denial disposition, never one success plus one denial.
    if ((st_q == M_VERDICT) && !owner_texture_q) begin
      geom_rsp_o.ok        = guard_rsp_i.ok && !guard_rsp_i.violation;
      geom_rsp_o.violation = guard_rsp_i.violation;
    end
  end

  // --------------------------------------------------------------- return --
  logic [6:0] raw_next_c;
  logic [5:0] texture_terminal_c;
  logic route_texture_c;
  logic route_mismatch_c;
  logic action_terminal_c, action_early_last_c, action_missing_last_c;
  logic exact_terminal_c, exact_last_mismatch_c;
  logic raw_usable_c;
  logic [63:0] packed_with_raw_c;
  logic texture_deny_intended_c, texture_deny_actual_c;
  logic texture_return_refusal_c, approved_bad_texture_c;

  always_comb begin
    raw_next_c = {1'b0, return_halfwords_q} + 7'd1;
    texture_terminal_c = `ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR(
                           expected_halfwords_q);
    route_texture_c = `ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR(
                        owner_texture_q, return_halfwords_q);
    route_mismatch_c = (route_texture_c != owner_texture_q);

    action_terminal_c = (raw_next_c == {1'b0, (owner_texture_q
                                              ? texture_terminal_c
                                              : expected_halfwords_q)});
    action_early_last_c = engine1_raw16_valid_i && engine1_raw16_last_i
                       && !action_terminal_c;
    action_missing_last_c = engine1_raw16_valid_i && action_terminal_c
                          && !engine1_raw16_last_i;
    exact_terminal_c = (raw_next_c == {1'b0, expected_halfwords_q});
    exact_last_mismatch_c = engine1_raw16_valid_i
                          && (engine1_raw16_last_i != exact_terminal_c);
    raw_usable_c = (st_q == M_DATA) && engine1_raw16_valid_i
                 && (raw_next_c <= {1'b0, (owner_texture_q
                                          ? texture_terminal_c
                                          : expected_halfwords_q)});

    packed_with_raw_c = pack_data_q;
    unique case (pack_halfwords_q)
      2'd0: packed_with_raw_c[15:0]  = engine1_raw16_data_i;
      2'd1: packed_with_raw_c[31:16] = engine1_raw16_data_i;
      2'd2: packed_with_raw_c[47:32] = engine1_raw16_data_i;
      default: packed_with_raw_c[63:48] = engine1_raw16_data_i;
    endcase

    texture_deny_intended_c = (st_q == M_VERDICT) && owner_texture_q
                            && guard_rsp_i.violation;
    texture_deny_actual_c = `ZHAO_RENDER_ASSET_DENY_PULSE_EXPR(
                              texture_deny_intended_c);
    approved_bad_texture_c = (st_q == M_VERDICT) && owner_texture_q
                           && guard_rsp_i.ok && !guard_rsp_i.violation
                           && !selected_shape_ok_q;
    texture_return_refusal_c = (st_q == M_DATA) && owner_texture_q
                             && (action_early_last_c
                                 || action_missing_last_c);

    texture_fill_data_valid_o = raw_usable_c && route_texture_c
                              && !action_early_last_c
                              && !action_missing_last_c;
    texture_fill_data_o       = engine1_raw16_data_i;
    texture_fill_refused_o    = texture_deny_actual_c
                              || approved_bad_texture_c
                              || texture_return_refusal_c;

    geom_beat_valid_o = raw_usable_c && !route_texture_c
                      && (pack_halfwords_q == 2'd3);
    geom_beat_data_o  = packed_with_raw_c;
    // Logical LAST is generated only on the exact action count; raw LAST is
    // checked independently, so a missing physical LAST cannot move the count.
    geom_beat_last_o  = geom_beat_valid_o && action_terminal_c;
  end

  // -------------------------------------------------------------- detectors --
  logic selected_source_missing_c, selected_source_changed_c;
  logic offer_source_fault_c;
  logic verdict_any_c, verdict_unsolicited_c, verdict_both_c;
  logic accept_wrong_state_c;
  logic raw_wrong_state_c, raw_last_without_valid_c, drain_surplus_c;
  logic wrong_count_c, pack_shape_fault_c, denial_silenced_c;
  logic approved_bad_shape_c;
  logic protocol_event_c, structural_event_c;

  always_comb begin
    selected_source_missing_c = 1'b0;
    selected_source_changed_c = 1'b0;
    if (st_q == M_OFFER) begin
      if (owner_texture_q) begin
        selected_source_missing_c = !texture_fill_valid_i;
        selected_source_changed_c = texture_fill_valid_i
                                  && (texture_fill_addr_i
                                      != source_texture_addr_q);
      end else begin
        selected_source_missing_c = !geom_req_i.valid;
        selected_source_changed_c = geom_req_i.valid
                                  && (geom_req_i != source_geom_req_q);
      end
    end
    offer_source_fault_c = (selected_source_missing_c
                            || selected_source_changed_c)
                           && !offer_source_faulted_q;

    verdict_any_c = guard_rsp_i.ok || guard_rsp_i.violation;
    verdict_unsolicited_c = verdict_any_c && (st_q != M_VERDICT);
    verdict_both_c = (st_q == M_VERDICT) && guard_rsp_i.ok
                   && guard_rsp_i.violation;
    accept_wrong_state_c = guard_accept_c && (st_q != M_OFFER);

    raw_wrong_state_c = engine1_raw16_valid_i
                      && (st_q != M_DATA) && (st_q != M_DRAIN_BAD);
    raw_last_without_valid_c = engine1_raw16_last_i
                             && !engine1_raw16_valid_i;
    drain_surplus_c = (st_q == M_DRAIN_BAD) && engine1_raw16_valid_i;
    wrong_count_c = (st_q == M_DATA) && engine1_raw16_valid_i
                  && ((raw_next_c > {1'b0, expected_halfwords_q})
                      || exact_last_mismatch_c);
    pack_shape_fault_c = (st_q == M_DATA) && engine1_raw16_valid_i
                       && !owner_texture_q && exact_terminal_c
                       && (pack_halfwords_q != 2'd3);
    denial_silenced_c = texture_deny_intended_c
                      && !texture_deny_actual_c;
    approved_bad_shape_c = (st_q == M_VERDICT) && guard_rsp_i.ok
                         && !guard_rsp_i.violation
                         && !selected_shape_ok_q;

    protocol_event_c = offer_source_fault_c
                     || verdict_unsolicited_c
                     || verdict_both_c
                     || accept_wrong_state_c
                     || raw_wrong_state_c
                     || raw_last_without_valid_c
                     || drain_surplus_c
                     || wrong_count_c
                     || ((st_q == M_DATA) && engine1_raw16_valid_i
                         && route_mismatch_c)
                     || pack_shape_fault_c
                     || denial_silenced_c
                     || approved_bad_shape_c;

    // Geometry has no typed short-return/refusal terminal.  Once its approved
    // route is short, long, mispacked, or diverted, only reset can make the
    // downstream consumer trustworthy again.
    structural_event_c = (st_q == M_DATA) && !owner_texture_q
                       && engine1_raw16_valid_i
                       && (exact_last_mismatch_c
                           || (raw_next_c > {1'b0, expected_halfwords_q})
                           || route_mismatch_c
                           || pack_shape_fault_c);
    if ((st_q == M_VERDICT) && !owner_texture_q && guard_rsp_i.ok
        && !guard_rsp_i.violation && !selected_shape_ok_q)
      structural_event_c = 1'b1;
  end

  // Quiet intentionally ignores the clearable protocol-fault level itself; that
  // is what permits a quiet frame clear.  Raw LAST without VALID is activity and
  // therefore cannot be hidden by a simultaneous clear.
  always_comb begin
    quiet_o = (st_q == M_IDLE)
           && !capture_live_q
           && !geom_req_i.valid
           && !texture_fill_valid_i
           && !guard_req_o.valid
           && !guard_rsp_i.ok
           && !guard_rsp_i.violation
           && !engine1_raw16_valid_i
           && !engine1_raw16_last_i
           && (pack_halfwords_q == 2'd0)
           && !geom_rsp_o.ready
           && !geom_rsp_o.ok
           && !geom_rsp_o.violation
           && !texture_fill_ready_o
           && !texture_fill_data_valid_o
           && !texture_fill_refused_o
           && !geom_beat_valid_o
           && !structural_fault_o;
  end

  initial begin : p_selector_contract
`ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
    $fatal(1, "ZHAO_RENDER_ASSET_MUX_MUTANT_SELECTOR_COLLISION: define exactly one selector");
`endif
  end
`ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  ZHAO_RENDER_ASSET_MUX_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE_SELECTOR
      u_render_asset_mux_selector_collision_compile_fail();
`endif

  // ------------------------------------------------------------------- FSM --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q                    <= M_IDLE;
      rr_last_texture_q       <= 1'b0;
      owner_texture_q         <= 1'b0;
      capture_live_q          <= 1'b0;
      selected_shape_ok_q     <= 1'b0;
      offer_source_faulted_q  <= 1'b0;
      guard_req_q             <= '0;
      source_geom_req_q       <= '0;
      source_texture_addr_q   <= 32'd0;
      expected_halfwords_q    <= 6'd0;
      return_halfwords_q      <= 6'd0;
      pack_halfwords_q        <= 2'd0;
      pack_data_q             <= 64'd0;
      protocol_fault_o        <= 1'b0;
      structural_fault_o      <= 1'b0;
      guard_accepted_o        <= 32'd0;
      guard_ok_o              <= 32'd0;
      guard_denied_o          <= 32'd0;
      geometry_accepted_o     <= 32'd0;
      geometry_refused_o      <= 32'd0;
      texture_accepted_o      <= 32'd0;
      texture_refused_o       <= 32'd0;
      contention_o            <= 32'd0;
      raw_halfwords_o         <= 32'd0;
      protocol_faults_o       <= 32'd0;
    end else begin
      // A recoverable fault clears only at a genuinely quiet boundary.  The
      // independent set below has priority if malformed activity shares an edge.
      if (frame_fault_clear_i && quiet_o)
        protocol_fault_o <= 1'b0;
      if (protocol_event_c) begin
        protocol_fault_o  <= 1'b1;
        protocol_faults_o <= protocol_faults_o + 32'd1;
      end
      if (structural_event_c)
        structural_fault_o <= 1'b1;

      if (engine1_raw16_valid_i)
        raw_halfwords_o <= raw_halfwords_o + 32'd1;

      // Physical acceptance evidence counts the actual interface event, even in
      // the duplicate-accept mutant.  Owner partitions use the retained capture.
      if (guard_accept_c) begin
        guard_accepted_o  <= guard_accepted_o + 32'd1;
        rr_last_texture_q <= owner_texture_q;
        if (owner_texture_q)
          texture_accepted_o <= texture_accepted_o + 32'd1;
        else
          geometry_accepted_o <= geometry_accepted_o + 32'd1;
      end

      if ((st_q == M_VERDICT) && guard_rsp_i.violation) begin
        guard_denied_o <= guard_denied_o + 32'd1;
        if (owner_texture_q) begin
          if (texture_deny_actual_c)
            texture_refused_o <= texture_refused_o + 32'd1;
        end else begin
          geometry_refused_o <= geometry_refused_o + 32'd1;
        end
      end else if ((st_q == M_VERDICT) && guard_rsp_i.ok) begin
        guard_ok_o <= guard_ok_o + 32'd1;
      end

      if (texture_return_refusal_c || approved_bad_texture_c)
        texture_refused_o <= texture_refused_o + 32'd1;

      if ((st_q == M_OFFER) && (selected_source_missing_c
                                || selected_source_changed_c))
        offer_source_faulted_q <= 1'b1;

      unique case (st_q)
        M_IDLE: begin
          return_halfwords_q <= 6'd0;
          pack_halfwords_q   <= 2'd0;
          pack_data_q        <= 64'd0;
          if (geom_wants_c || texture_wants_c) begin
            if (geom_wants_c && texture_wants_c)
              contention_o <= contention_o + 32'd1;
            owner_texture_q        <= pick_texture_c;
            capture_live_q         <= 1'b1;
            selected_shape_ok_q    <= picked_shape_ok_c;
            offer_source_faulted_q <= 1'b0;
            guard_req_q            <= picked_req_c;
            expected_halfwords_q   <= picked_halfwords_c;
            source_geom_req_q      <= geom_req_i;
            source_texture_addr_q  <= texture_fill_addr_i;
            st_q                   <= M_OFFER;
          end
        end

        M_OFFER: begin
          // Source lifetime has priority over acceptance. VALID/ready are already
          // suppressed combinationally on this cycle, and this edge destroys the
          // stale capture without advancing RR or any acceptance/disposition
          // counter. A later live offer is selected afresh in M_IDLE.
          if (selected_source_missing_c || selected_source_changed_c) begin
            st_q                   <= M_IDLE;
            owner_texture_q        <= 1'b0;
            capture_live_q         <= 1'b0;
            selected_shape_ok_q    <= 1'b0;
            offer_source_faulted_q <= 1'b0;
            guard_req_q            <= '0;
            source_geom_req_q      <= '0;
            source_texture_addr_q  <= 32'd0;
            expected_halfwords_q   <= 6'd0;
            return_halfwords_q     <= 6'd0;
            pack_halfwords_q       <= 2'd0;
            pack_data_q            <= 64'd0;
          end else if (guard_accept_c) begin
            st_q <= M_VERDICT;
          end
        end

        M_VERDICT: begin
          if (guard_rsp_i.violation) begin
            // Violation wins a simultaneous {ok,violation} and releases exactly
            // one local disposition.
            st_q                 <= M_IDLE;
            capture_live_q       <= 1'b0;
            guard_req_q          <= '0;
            expected_halfwords_q <= 6'd0;
          end else if (guard_rsp_i.ok) begin
            return_halfwords_q <= 6'd0;
            pack_halfwords_q   <= 2'd0;
            pack_data_q        <= 64'd0;
            if (!selected_shape_ok_q) begin
              // A real guard cannot approve CLIENT_NONE.  If an injected/broken
              // guard does, texture can terminate by refusal; geometry cannot.
              if (owner_texture_q) begin
                st_q           <= M_IDLE;
                capture_live_q <= 1'b0;
              end else begin
                st_q <= M_STRUCTURAL;
              end
            end else begin
              st_q <= M_DATA;
            end
          end
        end

        M_DATA: begin
          if (engine1_raw16_valid_i) begin
            return_halfwords_q <= raw_next_c[5:0];
            if (raw_usable_c && !route_texture_c) begin
              if (pack_halfwords_q == 2'd3) begin
                pack_halfwords_q <= 2'd0;
                pack_data_q      <= 64'd0;
              end else begin
                pack_halfwords_q <= pack_halfwords_q + 2'd1;
                pack_data_q      <= packed_with_raw_c;
              end
            end

            if (owner_texture_q) begin
              // The action threshold is mutant-selectable; the exact eight-word
              // invariant above is not, so beat-7/beat-9 defects remain visible.
              if (engine1_raw16_last_i) begin
                st_q <= M_IDLE;
                capture_live_q <= 1'b0;
                pack_halfwords_q <= 2'd0;
                pack_data_q <= 64'd0;
              end else if (action_terminal_c) begin
                st_q <= M_DRAIN_BAD;
                pack_halfwords_q <= 2'd0;
                pack_data_q <= 64'd0;
              end
            end else if (structural_event_c) begin
              st_q <= M_STRUCTURAL;
            end else if (action_terminal_c && engine1_raw16_last_i) begin
              st_q <= M_IDLE;
              capture_live_q <= 1'b0;
              pack_halfwords_q <= 2'd0;
              pack_data_q <= 64'd0;
            end
          end
        end

        M_DRAIN_BAD: begin
          // Every word is discarded.  Only VALID && LAST retires the malformed
          // physical return; LAST alone is itself a protocol fault and cannot.
          if (engine1_raw16_valid_i && engine1_raw16_last_i) begin
            st_q                 <= M_IDLE;
            capture_live_q       <= 1'b0;
            return_halfwords_q   <= 6'd0;
            pack_halfwords_q     <= 2'd0;
            pack_data_q          <= 64'd0;
          end
        end

        M_STRUCTURAL: begin
          // Reset-only by contract: no quiet, request, ready, or invented beat.
          st_q <= M_STRUCTURAL;
        end

        default: begin
          st_q <= M_STRUCTURAL;
          structural_fault_o <= 1'b1;
        end
      endcase
    end
  end

endmodule

`undef ZHAO_RENDER_ASSET_GUARD_VALID_EXPR
`undef ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR
`undef ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR
`undef ZHAO_RENDER_ASSET_DENY_PULSE_EXPR
`ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `undef ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
`endif
`ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
  `undef ZHAO_RENDER_ASSET_MUTANT_SELECTED
`endif
`default_nettype wire
