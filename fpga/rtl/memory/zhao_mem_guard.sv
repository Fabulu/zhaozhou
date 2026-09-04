// zhao_mem_guard.sv — region/ownership checker between EVERY fabric VRAM
// client and the memory system (plan W2.5). Law: spec/memory_rules.md §5;
// contract design/contracts/MEM.GUARD.md.
//
// Phase-2 region map (mode-independent — both slots are sized for the
// LARGEST canvas, so a mode switch never moves a slot):
//   FB slot 0 : [0x0000_0000, 0x0003_C000)   245,760 B  (DRAM bank 0)
//   FB slot 1 : [0x0200_0000, 0x0203_C000)   245,760 B  (DRAM bank 1 —
//               the W2.7 bank split; see zhao_pkg ZHAO_FB_SLOT1_BASE)
//   everything else: unmapped — violation by construction.
//   ENFORCED-BY: tests/formal/mem_guard_no_escape.sby
//
// Ownership law:
//   * SCANOUT owns BOTH slots, READ-ONLY (a write is a violation).
//   * BLIT_DMA owns exactly the CMD-granted slot window
//     [slot_base, slot_base + blit_span) — blit_span = canvas_bytes(mode),
//     written by CMD.SCHEDULER at frame grant. Writes only (Phase-2 blit
//     never reads VRAM: a read is a violation by construction).
//     map_valid=0 (no grant this frame) => deny-all.
//     ENFORCED-BY: tests/formal/mem_guard_no_escape.sby
//   * ENGINE0/ENGINE1/DEBUG own nothing in Phase 2 (reserved ports) —
//     violation by construction.
//     ENFORCED-BY: tests/formal/mem_guard_no_escape.sby
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
  input  logic        blit_slot,     // 0/1: the leased FB slot
  input  logic [31:0] blit_span,     // granted bytes (canvas_bytes(mode))
  // WHICH WRITER HOLDS THE LEASE. 0 = DEBUG.FRAMEBLIT, 1 = RASTER.FBWRITE.
  // Two blocks now write an inactive framebuffer slot and they share the
  // SPATIAL window but not the TEMPORAL permission: a second overlapping region
  // entry would copy the same address law, cost more plumbing, and still not
  // stop them corrupting each other. VIDEO.SLOTMGR already guarantees one lease
  // at a time with a generation, so the lease is where the writer is named.
  //
  // A v1 frame uses the renderer or DebugFrameBlit, never both.
  //
  // ENFORCED-BY: tests/formal/formal_mem_guard.sv:a1_region
  input  logic        fb_writer,

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
  logic        scan_ok, blit_ok, asset_ok;
  logic        pass_ok;

  assign addr32   = {5'b0, req.addr};
  assign end32    = addr32 + {25'b0, req.len};   // 32-bit: cannot wrap the map
  assign len_ok   = (req.len >= 7'd1) && (req.len <= 7'd64);
  assign be_ok    = (req.be == mask_of(req.len));
  assign shape_ok = len_ok && be_ok;

  // scanout: read-only, within EITHER FB slot. The slots are DISJOINT
  // regions since the bank split (zhao_pkg ZHAO_FB_SLOT1_BASE note): the
  // old single-comparison form (end <= SLOT1+SPAN) relied on the slots
  // being contiguous and would have admitted reads from the hole between
  // them. A <=64-B request can never bridge from slot 0 into slot 1, so
  // per-slot containment is exact.
  assign scan_ok = !req.write
                 && ((end32 <= ZHAO_FB_SLOT0_BASE + ZHAO_FB_SLOT_SPAN)
                     || ((addr32 >= ZHAO_FB_SLOT1_BASE)
                         && (end32 <= ZHAO_FB_SLOT1_BASE + ZHAO_FB_SLOT_SPAN)));

  // blit: write-only, within the granted slot window.
  // blit_span is clamped to the slot's own span before use. CMD.SCHEDULER is
  // specified to write canvas_bytes(mode) (<= ZHAO_FB_SLOT_SPAN), but the
  // guard must not TRUST that: an over-large span made blit_base + blit_span
  // wrap 32 bits, and a wrapped window admitted writes far outside the map —
  // an escape, in the one block whose entire contract is that no escape
  // exists. Clamping keeps every legal request legal (they never exceed the
  // slot) and closes the wrap by construction. Removing the clamp fails
  // the proof again (design/formal_runs.yml notes).
  // ENFORCED-BY: tests/formal/mem_guard_no_escape.sby
  logic [31:0] blit_base, blit_end, blit_span_eff;
  assign blit_base     = blit_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE;
  assign blit_span_eff = (blit_span > ZHAO_FB_SLOT_SPAN) ? ZHAO_FB_SLOT_SPAN : blit_span;
  assign blit_end      = blit_base + blit_span_eff;
  // The window check, shared by both writers. `fb_window_ok` is the name the
  // ruling gives it; `blit_ok` is kept as the identifier so the proof's own
  // wording still lines up with the code it proves.
  assign blit_ok       = req.write && map_valid
                         && (addr32 >= blit_base) && (end32 <= blit_end);

  // asset pool: READ-ONLY, ENGINE1's geometry region (spec/memory_rules.md 5f).
  // This is the Phase-3 extension the Phase-2 note promised, and it is the one
  // thing standing between the console and its geometry front end: every
  // MESHFETCH descriptor read was landing on `default: pass_ok = 1'b0`.
  //
  // A THIRD WINDOW IS OPENED HERE, unlike the ENGINE0 change which admitted a
  // client to an existing one -- so it is stated plainly rather than folded in.
  // What keeps the no-escape guarantee intact is that the window is
  // READ-ONLY and its bounds are CONSTANTS: `!req.write` means nothing in this
  // region can alter a frame buffer, and BASE + SPAN is 0x0800_0000 exactly,
  // computed at elaboration, so the wrap the blit clamp exists to prevent
  // cannot arise here. No map input is consulted, so unlike the blit window
  // this one is not frame-scoped and does not depend on `map_valid`.
  // ENFORCED-BY: tests/formal/mem_guard_no_escape.sby
  assign asset_ok = !req.write
                  && (addr32 >= ZHAO_GEOM_ASSET_BASE)
                  && (end32  <= ZHAO_GEOM_ASSET_BASE + ZHAO_GEOM_ASSET_SPAN);

  // ONE WINDOW, ONE OWNER AT A TIME. Both writers are checked against the same
  // clamped slot span; what separates them is which one the lease names. A
  // writer without the lease is refused exactly as a request outside the window
  // is, and the region this block guarantees nothing escapes is unchanged.
  always_comb begin
    unique case (req.client)
      ZHAO_CLIENT_SCANOUT:  pass_ok = shape_ok && scan_ok;
      ZHAO_CLIENT_BLIT_DMA: pass_ok = shape_ok && blit_ok && (fb_writer == 1'b0);
      ZHAO_CLIENT_ENGINE0:  pass_ok = shape_ok && blit_ok && (fb_writer == 1'b1);
      ZHAO_CLIENT_ENGINE1:  pass_ok = shape_ok && asset_ok;
      default: pass_ok = 1'b0;      // DEBUG still owns nothing
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

  // `rsp` gets exactly ONE kind of driver. Continuously assigning rsp.ready
  // while driving rsp.ok/rsp.violation from the always_ff below mixes a
  // continuous and a procedural driver on one variable, which IEEE 1800
  // forbids (a variable may be written by procedural statements OR by a
  // single continuous assignment, never both) — packed struct fields are not
  // separate variables. Tools that accept it resolve the struct
  // inconsistently: in the elaborated formal model arb_req.valid became
  // unreachable, which silently made the no-escape assertions VACUOUS.
  // The verdict bits are registered here and the whole struct is assembled
  // by this single always_comb.
  logic rsp_ok_q, rsp_violation_q;
  always_comb begin
    rsp.ready     = !fwd_active;   // level; verdict 1 cycle after accept
    rsp.ok        = rsp_ok_q;
    rsp.violation = rsp_violation_q;
  end

  // ------------------------------------------------------------- seq core --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fwd_active          <= 1'b0;
      fwd_req             <= '0;
      rsp_ok_q            <= 1'b0;
      rsp_violation_q     <= 1'b0;
      guard_violation     <= 1'b0;
      guard_violations    <= 32'd0;
      guard_violation_req <= '0;
    end else begin
      // defaults: one-cycle verdict pulses
      rsp_ok_q         <= 1'b0;
      rsp_violation_q  <= 1'b0;
      guard_violation  <= 1'b0;

      // THE COUNTER FOLLOWS THE PULSE, NOT THE DECISION.
      //
      // `guard_violations` used to increment in the same cycle as the verdict,
      // which put the whole range-check chain -- the requester's address, the
      // end address, four bounds compares, the client case -- on the ENABLE of
      // a 32-bit register bank. The full negative-slack census made it the
      // largest remaining family in the composed shell:
      //
      //   1,383 paths  scanout|fetch -> mem_guard|guard_violations   -0.195
      //
      // It counts the same events either way. `guard_violation` is already the
      // registered one-cycle pulse for exactly this verdict, so counting off
      // IT costs one cycle of latency on an observability counter and takes
      // the comparison chain off the counter entirely.
      //
      // The SAFETY path is untouched: rsp_ok_q, rsp_violation_q, fwd_active
      // and fwd_req still resolve in the accepting cycle, which is what
      // tests/formal/mem_guard_no_escape.sby proves about.
      if (guard_violation) guard_violations <= guard_violations + 32'd1;

      // forwarded request leaves when the arbiter accepts it
      if (fwd_active && arb_rsp.grant) fwd_active <= 1'b0;

      // accept at most one request per cycle (rsp.ready level above)
      if (req.valid && !fwd_active) begin
        if (pass_ok) begin
          rsp_ok_q          <= 1'b1;
          fwd_active        <= 1'b1;
          fwd_req.write     <= req.write;
          fwd_req.client    <= req.client;
          fwd_req.addr      <= req.addr;
          fwd_req.len       <= req.len;
        end else begin
          rsp_violation_q     <= 1'b1;
          guard_violation     <= 1'b1;
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
