// zhao_mem_guard.sv — region/ownership checker between EVERY fabric VRAM
// client and the memory system (plan W2.5). Law: spec/memory_rules.md §5;
// contract design/contracts/MEM.GUARD.md.
//
// Phase-2 region map (mode-independent — both slots are sized for the
// LARGEST canvas, so a mode switch never moves a slot):
//   FB slot 0 : [0x0000_0000, 0x0003_C000)   245,760 B
//   FB slot 1 : [0x0003_C000, 0x0007_8000)   245,760 B
//   everything else: unmapped — violation by construction.
//
// Ownership law:
//   * SCANOUT owns BOTH slots, READ-ONLY (a write is a violation).
//   * BLIT_DMA owns exactly the CMD-granted slot window
//     [slot_base, slot_base + blit_span) — blit_span = canvas_bytes(mode),
//     written by CMD.SCHEDULER at frame grant. Writes only (Phase-2 blit
//     never reads VRAM: a read is a violation by construction).
//     map_valid=0 (no grant this frame) => deny-all.
//   * ENGINE0/ENGINE1/DEBUG own nothing in Phase 2 (reserved ports) —
//     violation by construction.
//
// Request law: len 1..64 bytes; byte_enable must be the FULL contiguous mask
// over [addr, addr+len) (Phase-2 clients issue whole spans; partial-word
// tails are a Phase-3 map concern). Span arithmetic is done at 32 bits so
// addr+len can never wrap past the map.
//
// Verdict: fixed 1 cycle (registered rsp). A passing request is forwarded to
// the arbiter port and held there until the arbiter grants it (ready/valid
// end-to-end — the guard never drops a legal request, never invents one).
// A denied request is ANSWERED (guard_violation pulse + count + latched
// trace record, charter §29-17 lane) and NOTHING is forwarded — the formal
// property mem_guard_no_escape proves no violating request ever reaches the
// arbiter port and no partial forward exists.
//
// Conservative SystemVerilog subset only (charter §2). Lint: clean under
// `verilator --lint-only -Wall` (lint_mem_guard CTest).

module zhao_mem_guard
  import zhao_pkg::*;
(
  input  logic clk,
  input  logic rst_n,

  // client port (muxed upstream; the client field identifies the owner)
  input  zhao_guard_req_t req,
  output zhao_guard_rsp_t rsp,

  // region map inputs (CMD.SCHEDULER grants at frame start; deny-all below)
  input  logic        map_valid,     // a blit grant exists this frame
  input  logic        blit_slot,     // 0/1: the granted FB slot
  input  logic [31:0] blit_span,     // granted bytes (canvas_bytes(mode))

  // forwarded side (guard -> MEM.VRAM.ARBITER), ready/valid
  output zhao_arb_req_t arb_req,
  input  zhao_arb_rsp_t arb_rsp,

  // events / trace (violations are not a catalog counter; they trace to the
  // harness with the full request — the saved-failing-vector lane)
  output logic           guard_violation,      // one-cycle pulse
  output logic [31:0]    guard_violations,
  output zhao_guard_req_t guard_violation_req   // latched violating request
);

  // ------------------------------------------------------------ verdict ----
  // full contiguous byte mask over len bytes
  function automatic logic [63:0] mask_of(input logic [6:0] len_b);
    mask_of = '0;
    for (int b = 0; b < 64; b++) begin
      if (b < len_b) mask_of[b] = 1'b1;
    end
  endfunction

  logic [31:0] addr32, end32;
  logic        len_ok, be_ok, shape_ok;
  logic        scan_ok, blit_ok;
  logic        pass_ok;

  assign addr32   = {5'b0, req.addr};
  assign end32    = addr32 + {25'b0, req.len};   // 32-bit: cannot wrap the map
  assign len_ok   = (req.len >= 7'd1) && (req.len <= 7'd64);
  assign be_ok    = (req.be == mask_of(req.len));
  assign shape_ok = len_ok && be_ok;

  // scanout: read-only, within the two FB slots [0, 0x78000)
  assign scan_ok = !req.write && (end32 <= ZHAO_FB_SLOT1_BASE + ZHAO_FB_SLOT_SPAN);

  // blit: write-only, within the granted slot window
  logic [31:0] blit_base, blit_end;
  assign blit_base = blit_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE;
  assign blit_end  = blit_base + blit_span;
  assign blit_ok   = req.write && map_valid
                     && (addr32 >= blit_base) && (end32 <= blit_end);

  always_comb begin
    unique case (req.client)
      ZHAO_CLIENT_SCANOUT: pass_ok = shape_ok && scan_ok;
      ZHAO_CLIENT_BLIT_DMA: pass_ok = shape_ok && blit_ok;
      default: pass_ok = 1'b0;      // engines/debug own nothing in Phase 2
    endcase
  end

  // --------------------------------------------------------- forwarding ----
  // a passing request is latched and presented to the arbiter until granted;
  // ready/valid end-to-end: the guard is ready exactly when its forwarding
  // stage is free (an offered request is held upstream, never dropped)
  logic           fwd_active;
  zhao_arb_req_t  fwd_req;

  always_comb begin
    arb_req       = fwd_req;
    arb_req.valid = fwd_active;
  end
  assign rsp.ready = !fwd_active;   // level; verdict 1 cycle after accept

  // ------------------------------------------------------------- seq core --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fwd_active          <= 1'b0;
      fwd_req             <= '0;
      rsp.ok              <= 1'b0;
      rsp.violation       <= 1'b0;
      guard_violation     <= 1'b0;
      guard_violations    <= 32'd0;
      guard_violation_req <= '0;
    end else begin
      // defaults: one-cycle verdict pulses
      rsp.ok           <= 1'b0;
      rsp.violation    <= 1'b0;
      guard_violation  <= 1'b0;

      // forwarded request leaves when the arbiter accepts it
      if (fwd_active && arb_rsp.grant) fwd_active <= 1'b0;

      // accept at most one request per cycle (rsp.ready level above)
      if (req.valid && !fwd_active) begin
        if (pass_ok) begin
          rsp.ok            <= 1'b1;
          fwd_active        <= 1'b1;
          fwd_req.write     <= req.write;
          fwd_req.client    <= req.client;
          fwd_req.addr      <= req.addr;
          fwd_req.len       <= req.len;
        end else begin
          rsp.violation       <= 1'b1;
          guard_violation     <= 1'b1;
          guard_violations    <= guard_violations + 32'd1;
          guard_violation_req <= req;   // full request traced to the harness
        end
      end
    end
  end

  // arb_rsp.credits is not consumed here (the guard is request-path only;
  // the arbiter owns credit law) — keep the frozen port type and silence
  // the field-unused lint.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [7:0] unused_arb_credits;
  assign unused_arb_credits = arb_rsp.credits;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule
