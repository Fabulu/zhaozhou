// zhao_host_regwin.sv -- HOST.REGWIN: the HPS lightweight-bridge CSR aperture.
//
// Law: spec/memory_rules.md section 8 (the frozen host register map), owner
// ruling R51 (reports/OWNER-RULINGS-20260919-EVENING.md): "Ratify a HOST
// REGISTER WINDOW on the HPS lightweight bridge (a read/write CSR aperture with
// a frozen address map in spec/memory_rules.md). I19's histogram window and
// I45's readout are its first two tenants. It is guarded like every other
// client (a region per tenant, no escape)."
//
// ---------------------------------------------------------------------------
// WHAT THIS IS AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// It is NOT a second memory path. `zhao_hps_bridge` (spec/memory_rules.md 3) is
// the HPS-DDR BURST engine: 64-byte-aligned bursts of 64-bit beats, on the
// h2f DATA bridge, and every client of it MOVES BULK. A host that wants to read
// one 24-bit bin count cannot use it -- the bin lives in a block's registers,
// not in DRAM, and there is no agent that would copy it there.
//
// This is the other bridge. On the Cyclone V SoC the HPS exposes a SECOND,
// narrow, 32-bit port -- the lightweight HPS-to-FPGA bridge -- whose whole
// purpose is exactly this: the ARM reads and writes FPGA registers directly,
// one word at a time, with no descriptor and no DMA. It is a physical port of
// the part, in the same class as `pad_buttons_i` or `hps_req_*`: the console's
// edge, not a tie-off standing in for something unbuilt.
//
// ---------------------------------------------------------------------------
// THE NO-ESCAPE PROPERTY IS STRUCTURAL, NOT A COMPARISON
// ---------------------------------------------------------------------------
// MEM.GUARD proves no-escape by COMPARING an address against a region bound,
// which is the right shape there because the regions are runtime configuration.
// Here they are not: the map is frozen at elaboration, so the stronger form is
// available and is taken.
//
//     tenant index = h_addr_i[AW-1:TENANT_LSB]     -- selects the region
//     word offset  = h_addr_i[TENANT_LSB-1:2]      -- is what the tenant sees
//
// `t_woff_o` is OFFW = TENANT_LSB-2 bits WIDE. A tenant is therefore physically
// incapable of being handed an address outside its own region: there is no
// wire on which the tenant field could reach it. CLAUDE.md's law about
// detectors wired to operands that move together applies to bound-checks and
// not to this, because there is no check to get wrong -- the escape is not
// refused, it is unrepresentable.
//
// What IS checked, because it is representable:
//
//   * a tenant index >= NTENANT      -> refused, counted (`refused_unmapped_o`)
//   * a byte address with addr[1:0] != 0 -> refused, counted
//     (`refused_misaligned_o`). Same law as the burst bridge's malformed-burst
//     rejection: refused at the port, nothing issued downstream, never a wild
//     access.
//   * a tenant that REFUSES the access itself (an offset it does not map, or a
//     write to a read-only register) -> refused, counted (`refused_tenant_o`)
//   * a tenant that never answers    -> refused, counted (`refused_timeout_o`)
//
//     THE BOUND, EXACTLY, because the first version of this line said "after
//     ACK_LIMIT cycles" and that is not what the RTL does (found by a review
//     of the shipped block, 2026-09-20). `ackc_q` starts at 0 and the ack test
//     is taken BEFORE the limit test, so the tenant is given ACK_LIMIT + 1
//     opportunities and the refusal falls on the (ACK_LIMIT+1)-th cycle in the
//     state. That priority is deliberate -- an answer arriving on the limit
//     cycle IS an answer, and discarding it would manufacture a timeout out of
//     a tenant that met its deadline.
//
//     AND THERE ARE TWO BUDGETS, NOT ONE. `ackc_q` is cleared again on the ack
//     (S_SEL -> S_WAIT), so the acknowledge and the data each get their own
//     ACK_LIMIT + 1, and the worst case for one access is 2*(ACK_LIMIT+1)
//     cycles, 512 at the default. That is also deliberate -- "I have your
//     request" and "here is your answer" are two separate promises and a
//     tenant that keeps the first slowly has not broken the second -- but the
//     header read as a single budget and somebody sizing a host-side timeout
//     off it would have sized it half.
//
// EVERY ONE OF THOSE RETURNS A RESPONSE. A refused access is answered with
// `h_rvalid_o` + `h_err_o` and `h_rdata_o` = 0. IT NEVER HANGS -- owner ruling
// R20's law for MATERIAL.RESOLVE's denied fetch, and the same reason: a host
// bus that can stop answering is a console that locks up at the one moment
// somebody is trying to find out why.
//
// ---------------------------------------------------------------------------
// ONE ACCESS IN FLIGHT, AND WHY THAT IS NOT A NARROWING
// ---------------------------------------------------------------------------
// The lightweight bridge is a single-outstanding AXI-lite-class port used by an
// ARM doing register reads between frames. Pipelining it would buy throughput
// nobody asks for and would need a reorder buffer to keep tenant responses in
// request order. `h_ready_o` is low while an access is in flight, so the host
// simply waits; `stall_cycles_o` counts how long it waited, which is the number
// anyone sizing this later actually needs.
//
// ---------------------------------------------------------------------------
// WRITES: THE PATH IS REAL AND NO v1 TENANT ACCEPTS ONE
// ---------------------------------------------------------------------------
// Stated plainly so it is not read as a stub. The aperture carries `h_write_i`
// and `h_wdata_i` to the tenant, and each tenant decides. Both v1 tenants
// (MEASURE.HISTOGRAM's bins, DEBUG.TRACE's ring) are READ-ONLY, and a write to
// either is refused on `t_err_i` and counted -- because the one thing a host
// would want to WRITE, DEBUG.TRACE's arming, travels in the command stream
// instead: owner ruling R52 ratified `DebugTraceArm` 0xF003 for it, and ONE
// AUTHORITY PER LEVEL (ruling R18's principle) says it may not also be a CSR.
// The refusal is behaviour, not absence: it is counted, it is tested, and a
// third tenant that wants writes needs no change here.
//
// Conservative SystemVerilog subset only (charter section 2). No inline
// `for (genvar`, no module-scope `if` guard, explicit generate.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_host_regwin).
// ENFORCED-BY: tests/debug/host_regwin_directed.cpp

module zhao_host_regwin #(
    // Tenants in the map. The map is frozen in spec/memory_rules.md section 8;
    // this parameter says how many of its slots are POPULATED, and every slot
    // at or above it is refused.
    parameter int unsigned NTENANT    = 2,
    // Byte-address width of the whole aperture (16 -> 64 KiB).
    parameter int unsigned AW         = 16,
    // Region stride, log2 bytes (12 -> 4 KiB per tenant, 16 tenants in 64 KiB).
    parameter int unsigned TENANT_LSB = 12,
    parameter int unsigned CW         = 32,
    // Cycles a tenant may take to acknowledge before the access is refused.
    // READ THE HEADER'S "THE BOUND, EXACTLY": the tenant actually gets
    // ACK_LIMIT + 1 opportunities, and the acknowledge and the data have
    // SEPARATE budgets of that size, so one access can take 2*(ACK_LIMIT+1).
    parameter int unsigned ACK_LIMIT  = 255
) (
    input  logic clk,
    input  logic rst_n,

    // ---- host side: the HPS lightweight bridge -----------------------------
    input  logic          h_valid_i,
    input  logic          h_write_i,
    input  logic [AW-1:0] h_addr_i,     // byte address WITHIN the aperture
    input  logic [31:0]   h_wdata_i,
    output logic          h_ready_o,    // level: low while an access is in flight
    output logic          h_rvalid_o,   // one-cycle response pulse
    output logic [31:0]   h_rdata_o,
    output logic          h_err_o,      // qualified by h_rvalid_o

    // ---- tenant side -------------------------------------------------------
    // One-hot select. `t_woff_o` is the WORD offset inside the selected
    // tenant's region and is physically too narrow to leave it.
    output logic [NTENANT-1:0]        t_sel_o,
    output logic [TENANT_LSB-3:0]     t_woff_o,
    output logic                      t_write_o,
    output logic [31:0]               t_wdata_o,
    input  logic [NTENANT-1:0]        t_ack_i,     // tenant took the access
    input  logic [NTENANT-1:0]        t_rvalid_i,  // tenant returns data
    input  logic [NTENANT*32-1:0]     t_rdata_i,
    input  logic [NTENANT-1:0]        t_err_i,     // tenant refuses (with rvalid)

    // ---- evidence ----------------------------------------------------------
    output logic [CW-1:0] reads_o,
    output logic [CW-1:0] writes_o,
    output logic [CW-1:0] refused_unmapped_o,
    output logic [CW-1:0] refused_misaligned_o,
    output logic [CW-1:0] refused_tenant_o,
    output logic [CW-1:0] refused_timeout_o,
    output logic [CW-1:0] stall_cycles_o
);

  localparam int unsigned OFFW  = TENANT_LSB - 2;      // word-offset width
  localparam int unsigned TIDW  = AW - TENANT_LSB;     // tenant-index width
  localparam int unsigned SELW  = (NTENANT > 1) ? $clog2(NTENANT) : 1;
  localparam int unsigned ACKW  = $clog2(ACK_LIMIT + 1);

  localparam logic [CW-1:0] CNT_MAX = {CW{1'b1}};

  // Quartus 17.0 rejects a module-scope `if` elaboration guard; it must sit
  // inside an initial block (CLAUDE.md, 2026-09-08).
  // synthesis translate_off
  initial begin
    if (AW <= TENANT_LSB)
      $fatal(1, "zhao_host_regwin: AW (%0d) must exceed TENANT_LSB (%0d)",
             AW, TENANT_LSB);
    if (TENANT_LSB < 3)
      $fatal(1, "zhao_host_regwin: TENANT_LSB (%0d) leaves no word offset",
             TENANT_LSB);
    if (NTENANT == 0)
      $fatal(1, "zhao_host_regwin: NTENANT must be at least 1");
    if (NTENANT > (1 << TIDW))
      $fatal(1, "zhao_host_regwin: NTENANT (%0d) exceeds the %0d map regions",
             NTENANT, 1 << TIDW);
  end
  // synthesis translate_on

  function automatic logic [CW-1:0] sat_inc(input logic [CW-1:0] c);
    logic [CW:0] s;
    begin
      s = {1'b0, c} + {{CW{1'b0}}, 1'b1};
      sat_inc = s[CW] ? CNT_MAX : s[CW-1:0];
    end
  endfunction

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_SEL  = 2'd1;  // offering the access to the tenant
  localparam logic [1:0] S_WAIT = 2'd2;  // tenant took it; waiting for data
  localparam logic [1:0] S_RESP = 2'd3;  // one-cycle answer to the host

  logic [1:0]      st_q;
  logic [OFFW-1:0] woff_q;
  logic [SELW-1:0] sel_q;
  logic            write_q;
  logic [31:0]     wdata_q;
  logic [31:0]     rdata_q;
  logic            err_q;
  logic [ACKW-1:0] ackc_q;

  // ---- decode of the OFFERED address (combinational, IDLE only) ------------
  logic [TIDW-1:0] tid_c;
  logic            misaligned_c;
  logic            unmapped_c;

  assign tid_c        = h_addr_i[AW-1:TENANT_LSB];
  assign misaligned_c = (h_addr_i[1:0] != 2'b00);
  // A tenant index at or above NTENANT names a region the map has not
  // populated. Compared against a PARAMETER, so the comparison cannot be
  // desynchronised from the thing it guards the way a runtime bound can.
  assign unmapped_c   = ({{(32-TIDW){1'b0}}, tid_c} >= NTENANT);

  // ---- the tenant-facing offer --------------------------------------------
  integer si;
  always_comb begin
    t_sel_o = '0;
    if (st_q == S_SEL) begin
      for (si = 0; si < NTENANT; si = si + 1) begin
        if (si == {{(32-SELW){1'b0}}, sel_q}) t_sel_o[si] = 1'b1;
      end
    end
  end

  assign t_woff_o  = woff_q;
  assign t_write_o = write_q;
  assign t_wdata_o = wdata_q;

  // The selected tenant's response, chosen AFTER the registers so the mux is
  // one level and the selector cannot be a cycle out of step with the data it
  // steers (CLAUDE.md: a comparison whose two sides move on one enable is
  // blind; the cure here is to have exactly one enable and one source).
  logic        sel_ack_c;
  logic        sel_rvalid_c;
  logic        sel_err_c;
  logic [31:0] sel_rdata_c;

  assign sel_ack_c    = t_ack_i[sel_q];
  assign sel_rvalid_c = t_rvalid_i[sel_q];
  assign sel_err_c    = t_err_i[sel_q];
  assign sel_rdata_c  = t_rdata_i[{{(32-SELW){1'b0}}, sel_q} * 32 +: 32];

  assign h_ready_o  = (st_q == S_IDLE);
  assign h_rvalid_o = (st_q == S_RESP);
  assign h_rdata_o  = rdata_q;
  assign h_err_o    = err_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q                 <= S_IDLE;
      woff_q               <= '0;
      sel_q                <= '0;
      write_q              <= 1'b0;
      wdata_q              <= '0;
      rdata_q              <= '0;
      err_q                <= 1'b0;
      ackc_q               <= '0;
      reads_o              <= '0;
      writes_o             <= '0;
      refused_unmapped_o   <= '0;
      refused_misaligned_o <= '0;
      refused_tenant_o     <= '0;
      refused_timeout_o    <= '0;
      stall_cycles_o       <= '0;
    end else begin
      if (st_q != S_IDLE) stall_cycles_o <= sat_inc(stall_cycles_o);

      case (st_q)
        S_IDLE: begin
          if (h_valid_i) begin
            // Every accepted access is counted as what it ASKED to be, before
            // any verdict. A refused read is still a read the host attempted,
            // and a census that only counted the ones that worked would read
            // low -- the flattering direction.
            if (h_write_i) writes_o <= sat_inc(writes_o);
            else           reads_o  <= sat_inc(reads_o);

            woff_q  <= h_addr_i[TENANT_LSB-1:2];
            sel_q   <= h_addr_i[TENANT_LSB +: SELW];
            write_q <= h_write_i;
            wdata_q <= h_wdata_i;
            rdata_q <= '0;
            ackc_q  <= '0;

            if (misaligned_c) begin
              refused_misaligned_o <= sat_inc(refused_misaligned_o);
              err_q                <= 1'b1;
              st_q                 <= S_RESP;
            end else if (unmapped_c) begin
              refused_unmapped_o <= sat_inc(refused_unmapped_o);
              err_q              <= 1'b1;
              st_q               <= S_RESP;
            end else begin
              err_q <= 1'b0;
              st_q  <= S_SEL;
            end
          end
        end

        S_SEL: begin
          if (sel_ack_c) begin
            ackc_q <= '0;
            st_q   <= S_WAIT;
          end else if (ackc_q == ACKW'(ACK_LIMIT)) begin
            refused_timeout_o <= sat_inc(refused_timeout_o);
            err_q             <= 1'b1;
            rdata_q           <= '0;
            st_q              <= S_RESP;
          end else begin
            ackc_q <= ackc_q + ACKW'(1);
          end
        end

        S_WAIT: begin
          if (sel_rvalid_c) begin
            rdata_q <= sel_err_c ? 32'd0 : sel_rdata_c;
            err_q   <= sel_err_c;
            if (sel_err_c) refused_tenant_o <= sat_inc(refused_tenant_o);
            st_q    <= S_RESP;
          end else if (ackc_q == ACKW'(ACK_LIMIT)) begin
            refused_timeout_o <= sat_inc(refused_timeout_o);
            err_q             <= 1'b1;
            rdata_q           <= '0;
            st_q              <= S_RESP;
          end else begin
            ackc_q <= ackc_q + ACKW'(1);
          end
        end

        default: begin  // S_RESP
          st_q <= S_IDLE;
        end
      endcase
    end
  end

endmodule
