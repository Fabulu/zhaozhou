// zhao_texture_palette_res_v2.sv — Packet-B resident palette resolver.
//
// The unversioned palette leaf emits an unstallable one-cycle colour plus two
// verdict bits, leaving its caller to preserve the response token and CLUT index
// across the RAM latency.  Packet B makes that alignment structural: one exact
// response record enters, its palette binding is judged at acceptance, and one
// exact response record leaves through an elastic held register.
//
// At the selected TOKW=18, the response is exactly 66 bits:
//   {route_token18, status8, raw_index8, alpha8, RGB24}
// The raw index is both the CLUT address and the immutable sample-0 witness.  A
// stale or nonresident palette ORs SOURCE_REFUSED into status and returns loud
// magenta; it never borrows a current token, index, alpha, slot or generation.
//
// Palette programming retains the explicit BEGIN/WRITE/END protocol.  The RAM
// is deliberately not reset.  Residency and generation are the reset guards --
// and RESIDENCY is the one that guards the BEGIN.  A cold slot accepts ANY
// generation, ZERO INCLUDED; only a slot that is currently resident refuses a
// BEGIN at the generation it already advertises.
`default_nettype none

module zhao_texture_palette_res_v2 #(
    parameter int unsigned SLOTS   = 4,
    parameter int unsigned ENTRIES = 256,
    parameter int unsigned GENW    = 8,
    parameter int unsigned TOKW    = 18
) (
    input  var logic                         clk,
    input  var logic                         rst_n,

    // BEGIN(0), WRITE(1), END(2).  Packet B defines no acknowledgement channel.
    input  var logic                         ld_valid_i,
    output var logic                         ld_ready_o,
    input  var logic [1:0]                   ld_op_i,
    input  var logic [$clog2(SLOTS)-1:0]     ld_slot_i,
    input  var logic [GENW-1:0]              ld_gen_i,
    input  var logic [$clog2(ENTRIES)-1:0]   ld_idx_i,
    input  var logic [15:0]                  ld_rgb565_i,
    input  var logic                         ld_crc_ok_i,

    // Exact class-response tuple plus the resolved palette identity.
    input  var logic                         req_valid_i,
    output var logic                         req_ready_o,
    input  var logic [TOKW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                                                req_tuple_i,
    input  var logic [$clog2(SLOTS)-1:0]     req_slot_i,
    input  var logic [GENW-1:0]              req_gen_i,

    output var logic                         rsp_valid_o,
    input  var logic                         rsp_ready_i,
    output var logic [TOKW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                                                rsp_tuple_o,

    // data-plane and programming-plane structural observations
    output var logic                         idle_o,
    output var logic                         cfg_idle_o,

    // evidence
    output var logic [31:0]                  lookups_o,
    output var logic [31:0]                  stale_o,
    output var logic [31:0]                  cold_o,
    output var logic [31:0]                  err_write_outside_o,
    output var logic [31:0]                  err_same_gen_o,
    output var logic [31:0]                  err_incomplete_o,
    output var logic [31:0]                  err_crc_o,
    output var logic [31:0]                  loads_ok_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned TUPLE_W = TOKW + TEXTURE_RESULT_W;
  localparam int unsigned SLOTW   = $clog2(SLOTS);
  localparam int unsigned IDXW    = $clog2(ENTRIES);

  localparam logic [1:0] LD_BEGIN = 2'd0;
  localparam logic [1:0] LD_WRITE = 2'd1;
  localparam logic [1:0] LD_END   = 2'd2;


  initial begin : p_layout_contract
    if (TUPLE_W != 66)
      $fatal(1, "ZHAO_PALETTE_V2_PARAM_FIRE[TUPLE66]");
    if ((SLOTS < 2) || ((1 << SLOTW) != SLOTS))
      $fatal(1, "ZHAO_PALETTE_V2_PARAM_FIRE[SLOTS_POWER_OF_TWO]");
    // raw_index is an immutable eight-bit CLUT witness.  A smaller table would
    // silently discard address bits; a larger one could never be addressed by
    // the exact 66-bit packet.  Keep both the semantic count and its derived
    // address width pinned independently.
    if ((ENTRIES != 256) || (IDXW != 8))
      $fatal(1, "ZHAO_PALETTE_V2_PARAM_FIRE[ENTRIES256_IDXW8]");
    if (GENW != 8)
      $fatal(1, "ZHAO_PALETTE_V2_PARAM_FIRE[GENW8]");
  end

  // ---- resident storage ------------------------------------------------------
  logic [15:0] mem_q [SLOTS * ENTRIES];
  logic [GENW-1:0] generation_q [SLOTS];
  logic            resident_q   [SLOTS];

  logic                    loading_q;
  logic [SLOTW-1:0]        loading_slot_q;
  logic [GENW-1:0]         loading_gen_q;
  logic [ENTRIES-1:0]      seen_q;

  logic load_write_c;
  logic req_fire_c;
  assign ld_ready_o   = 1'b1;
  assign load_write_c = ld_valid_i && ld_ready_o && (ld_op_i == LD_WRITE) && loading_q;

  // ---- two-stage elastic lookup ---------------------------------------------
  // L1 captures the independently timed generation/residency verdict beside the
  // exact tuple.  RSP is the held 66-bit terminal record.
  logic                l1_valid_q;
  logic [TOKW + TEXTURE_RESULT_W-1:0] l1_tuple_q;
  logic [15:0]         l1_data_q;
  logic                l1_stale_q;
  logic                l1_resident_q;

  logic                rsp_valid_q;
  logic [TOKW + TEXTURE_RESULT_W-1:0] rsp_tuple_q;

  logic rsp_slot_ready_c;
  logic l1_slot_ready_c;
  logic begin_accept_c;
  logic begin_same_slot_c;
  logic req_stale_c;
  logic req_resident_c;

  always_comb begin
    rsp_slot_ready_c = !rsp_valid_q || rsp_ready_i;
    l1_slot_ready_c  = !l1_valid_q || rsp_slot_ready_c;
    req_ready_o      = l1_slot_ready_c;
    req_fire_c       = req_valid_i && req_ready_o;

    // THE ACCEPTANCE TERM IS WRITTEN ONCE AND READ TWICE.  The FSM below and
    // the same-edge invalidation here MUST agree about whether a BEGIN was
    // taken; two spellings of one condition is CLAUDE.md's detector-wired-to-
    // two-operands defect with the operands swapped, and it would let a
    // REFUSED begin invalidate a live binding, or an ACCEPTED one fail to.
    //
    // A BEGIN IS REFUSED ONLY WHEN IT WOULD REPLACE A *RESIDENT* BINDING WITH
    // CONTENT UNDER THE GENERATION THAT BINDING ALREADY ADVERTISES.  That is
    // the whole of the fault `err_same_gen_o` exists to catch: a consumer
    // holding {slot, generation} cannot tell the old bytes from the new, so
    // the replacement is an ABA hazard and must be a verdict, never a load.
    //
    // REPAIRED 2026-09-26 (gz/i13close).  The guard used to compare the
    // generation ALONE, and `generation_q[slot]` RESETS TO ZERO -- so a slot
    // that had never been loaded refused generation ZERO, and zero was the one
    // generation no producer could hand this block in a single pass.  A real
    // palette producer allocating generations from its own reset counter wants
    // exactly that value first, and the previous shape forced it either to
    // skip zero (a special case nobody could re-derive) or to load some other
    // generation and reload at zero (a contortion a bench can reach and a
    // machine should not have to).  RESIDENCY is the reset guard this block's
    // own header names; the generation was doing residency's job.
    //
    // `err_same_gen_o` LOSES NOTHING: a resident slot re-BEGUN at its own
    // generation still fires it, which is the only state in which the fault
    // it describes can exist, and `texture_palette_res_v2_directed`'s control
    // now fires it that way instead of by reusing the RESET generation.
    begin_accept_c = ld_valid_i && ld_ready_o && (ld_op_i == LD_BEGIN)
                  && !(resident_q[ld_slot_i] && (ld_gen_i == generation_q[ld_slot_i]));
    // An accepted BEGIN invalidates this slot on the same edge.  Compare that
    // independently from the registered state so a same-edge lookup cannot see
    // the palette being replaced as fresh.
    begin_same_slot_c = begin_accept_c && (ld_slot_i == req_slot_i);
    req_stale_c   = (generation_q[req_slot_i] != req_gen_i) || begin_same_slot_c;
    req_resident_c = resident_q[req_slot_i] && !begin_same_slot_c;

    rsp_valid_o = rsp_valid_q;
    rsp_tuple_o = rsp_tuple_q;
    idle_o      = !l1_valid_q && !rsp_valid_q;
    cfg_idle_o  = !loading_q;
  end

  // One write and one synchronous read.  No reset may touch this process or the
  // array, preserving the inference-compatible memory shape.
  always_ff @(posedge clk) begin
    if (load_write_c)
      mem_q[{loading_slot_q, ld_idx_i}] <= ld_rgb565_i;
    if (req_fire_c)
      l1_data_q <= mem_q[{req_slot_i,
                           req_tuple_i[TEXTURE_RESULT_SAMPLE0_INDEX_LO +: IDXW]}];
  end

  function automatic logic [23:0] expand_rgb565(input logic [15:0] value);
    logic [4:0] r5;
    logic [5:0] g6;
    logic [4:0] b5;
    begin
      r5 = value[15:11];
      g6 = value[10:5];
      b5 = value[4:0];
      expand_rgb565 = {{r5, r5[4:2]}, {g6, g6[5:4]}, {b5, b5[4:2]}};
    end
  endfunction

  logic [7:0]  l1_status_c;
  logic        l1_bad_c;
  logic [23:0] l1_rgb_c;
  logic [TOKW + TEXTURE_RESULT_W-1:0] resolved_tuple_c;

  always_comb begin
    l1_bad_c    = (l1_tuple_q[TEXTURE_RESULT_STATUS_LO +: 8] != 8'd0)
               || l1_stale_q || !l1_resident_q;
    l1_status_c = l1_tuple_q[TEXTURE_RESULT_STATUS_LO +: 8]
                | ((l1_stale_q || !l1_resident_q)
                   ? (8'(1) << TEXTURE_STATUS_SOURCE_REFUSED_BIT) : 8'h00);
    l1_rgb_c = l1_bad_c ? 24'hFF00FF : expand_rgb565(l1_data_q);

    // Preserve every non-RGB field from the same accepted tuple.  In particular,
    // the addressed raw index is not reconstructed after this latency.
    resolved_tuple_c = l1_tuple_q;
    resolved_tuple_c[TEXTURE_RESULT_STATUS_LO +: 8] = l1_status_c;
    resolved_tuple_c[TEXTURE_RESULT_RGB_LO +: 24]  = l1_rgb_c;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      loading_q           <= 1'b0;
      loading_slot_q      <= '0;
      loading_gen_q       <= '0;
      seen_q               <= '0;
      l1_valid_q           <= 1'b0;
      l1_tuple_q           <= '0;
      l1_stale_q           <= 1'b0;
      l1_resident_q        <= 1'b0;
      rsp_valid_q          <= 1'b0;
      rsp_tuple_q          <= '0;
      lookups_o            <= 32'd0;
      stale_o              <= 32'd0;
      cold_o               <= 32'd0;
      err_write_outside_o  <= 32'd0;
      err_same_gen_o       <= 32'd0;
      err_incomplete_o     <= 32'd0;
      err_crc_o            <= 32'd0;
      loads_ok_o           <= 32'd0;
      for (int slot = 0; slot < SLOTS; slot++) begin
        generation_q[slot] <= '0;
        resident_q[slot]   <= 1'b0;
      end
    end else begin
      // Programming protocol.  The generation changes and residency clears at
      // BEGIN, not END, so old bindings become stale before the first write.
      if (ld_valid_i && ld_ready_o) begin
        unique case (ld_op_i)
          LD_BEGIN: begin
            if (!begin_accept_c) begin
              err_same_gen_o <= err_same_gen_o + 32'd1;
            end else begin
              generation_q[ld_slot_i] <= ld_gen_i;
              resident_q[ld_slot_i]   <= 1'b0;
              loading_q               <= 1'b1;
              loading_slot_q          <= ld_slot_i;
              loading_gen_q           <= ld_gen_i;
              seen_q                  <= '0;
            end
          end

          LD_WRITE: begin
            if (!loading_q)
              err_write_outside_o <= err_write_outside_o + 32'd1;
            else
              seen_q[ld_idx_i] <= 1'b1;
          end

          LD_END: begin
            if (!loading_q || (ld_slot_i != loading_slot_q)
                           || (ld_gen_i != loading_gen_q)) begin
              err_write_outside_o <= err_write_outside_o + 32'd1;
            end else begin
              loading_q <= 1'b0;
              if (!ld_crc_ok_i)
                err_crc_o <= err_crc_o + 32'd1;
              else if (seen_q != {ENTRIES{1'b1}})
                err_incomplete_o <= err_incomplete_o + 32'd1;
              else begin
                resident_q[loading_slot_q] <= 1'b1;
                loads_ok_o <= loads_ok_o + 32'd1;
              end
            end
          end

          default: err_write_outside_o <= err_write_outside_o + 32'd1;
        endcase
      end

      // Elastic response stage.  A stalled response changes neither valid nor a
      // single payload bit; simultaneous pop/reload remains one-per-clock.
      if (rsp_slot_ready_c) begin
        rsp_valid_q <= l1_valid_q;
        if (l1_valid_q)
          rsp_tuple_q <= resolved_tuple_c;
      end

      if (l1_slot_ready_c) begin
        l1_valid_q <= req_fire_c;
        if (req_fire_c) begin
          l1_tuple_q    <= req_tuple_i;
          l1_stale_q    <= req_stale_c;
          l1_resident_q <= req_resident_c;
          lookups_o     <= lookups_o + 32'd1;
          if (req_stale_c)
            stale_o <= stale_o + 32'd1;
          else if (!req_resident_c)
            cold_o <= cold_o + 32'd1;
        end
      end
    end
  end

endmodule : zhao_texture_palette_res_v2

`default_nettype wire
