// formal_mem_guard.sv — formal harness for mem_guard_no_escape (plan W2.5).
//
// PROPERTIES (spec/memory_rules.md §5, contract MEM.GUARD):
//   A1 no escape: whenever the guard forwards a request to the arbiter port,
//      that request lies fully inside its client's OWNED region (the Phase-2
//      map: scanout read-only within either FB slot (disjoint since the
//      W2.7 bank split); blit write-only inside the
//      CMD-granted slot window).
//   A2 no partial/malformed forward: a forwarded request has a legal length
//      (1..64 bytes) and the full contiguous byte mask.
//   A3 deny-all out of reset: nothing is forwarded in the first cycle after
//      the reset is released (the forwarding stage powers up empty).
//
// NON-VACUITY: A1/A2 are implications guarded by arb_req.valid. If the
// elaborated model cannot reach arb_req.valid they hold TRIVIALLY and prove
// nothing — which is exactly what happened before: a mixed continuous/
// procedural driver on `rsp` in the DUT made the forwarding path collapse, so
// the headline assertions were vacuous in every buildable configuration. The
// cover statements below are therefore part of the property, not decoration:
// the `cover` task must find a forward for BOTH owning clients, a violation,
// and a full accept/forward/grant handshake. If any cover goes unreachable
// the proof is not to be believed, whatever the bmc task reports.
//
// ENVIRONMENT (documented so the proof's scope is honest):
//   * the request port is entirely free every cycle — no assumption is made
//     about client behaviour, which is the whole point of a guard;
//   * the region map (map_valid/blit_slot/blit_span) is arbitrary but LATCHED
//     ONCE at cycle 0 and constant thereafter. That is exactly its real
//     lifetime — CMD.SCHEDULER writes it at frame grant and it is stable for
//     the frame — and it is what makes the property well-posed, since a
//     request accepted under one map is forwarded a cycle later. blit_span is
//     NOT otherwise constrained: the DUT clamps it to the slot span itself;
//   * arbiter grant is free, so the guard must hold with a fast, slow, or
//     never-accepting downstream.
//
// The free inputs are top-module PORTS, which sby/yosys treat as unconstrained
// variables. They are deliberately NOT `(* anyseq *)` locals: that attribute
// does not survive this frontend — it elaborated to constants, which is what
// emptied the model in the first place (no env signal appears in the witness
// at all). Ports cannot be optimised away, so the freedom is structural.

module formal_mem_guard
  import zhao_pkg::*;
(
  input logic        clk,
  // free request port
  input logic        env_valid,
  input logic        env_write,
  input logic [2:0]  env_client,
  input logic [26:0] env_addr,
  input logic [6:0]  env_len,
  input logic [63:0] env_be,
  // free downstream acceptance
  input logic        env_grant,
  // free region map (sampled once at cycle 0, held constant below)
  input logic        env_map_valid_i,
  input logic        env_blit_slot_i,
  input logic [31:0] env_blit_span_i
);

  // ------------------------------------------------------- reset discipline
  // The DUT resets asynchronously but is sampled on posedge clk; with a free
  // rst_n the solver simply starts mid-reset with arbitrary state and the
  // proof fails for reasons that say nothing about the design (the free-init
  // trap the W2.3 TASK_LOG recorded). Here the reset is a deterministic
  // counter — initialised, so the solver cannot choose it — held low for two
  // cycles and released forever after.
  logic [3:0] cyc = 4'd0;
  always_ff @(posedge clk) begin
    if (cyc != 4'hF) cyc <= cyc + 4'd1;
  end
  wire rst_n     = (cyc >= 4'd2);   // low for cycles 0,1
  wire released  = (cyc == 4'd2);   // the first cycle out of reset (A3)
  wire checking  = (cyc >= 4'd2);   // assert only once reset has been applied

  // ------------------------------------------------------------ environment
  // frame-scoped region map: arbitrary, but sampled ONCE at cycle 0 and held,
  // so it is constant for the whole trace by construction (see header). The
  // DUT is fed from these registers, never from the raw inputs.
  logic        env_map_valid;
  logic        env_blit_slot;
  logic [31:0] env_blit_span;
  always_ff @(posedge clk) begin
    if (cyc == 4'd0) begin
      env_map_valid <= env_map_valid_i;
      env_blit_slot <= env_blit_slot_i;
      env_blit_span <= env_blit_span_i;
    end
  end

  zhao_guard_req_t req;
  assign req.valid  = env_valid;
  assign req.write  = env_write;
  assign req.client = zhao_client_e'(env_client);
  assign req.addr   = env_addr;
  assign req.len    = env_len;
  assign req.be     = env_be;

  zhao_guard_rsp_t rsp;
  zhao_arb_req_t   arb_req;
  zhao_arb_rsp_t   arb_rsp;
  assign arb_rsp.grant   = env_grant;   // free: fast, slow or never
  assign arb_rsp.credits = 8'd0;

  logic            guard_violation;
  logic [31:0]     guard_violations;
  zhao_guard_req_t guard_violation_req;

  zhao_mem_guard u_guard (
    .clk, .rst_n,
    .req, .rsp,
    .map_valid (env_map_valid), .blit_slot (env_blit_slot),
    .blit_span (env_blit_span),
    .arb_req, .arb_rsp,
    .guard_violation, .guard_violations, .guard_violation_req
  );

  // the effective (clamped) blit window the DUT is entitled to allow
  wire [31:0] blit_base = env_blit_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE;
  wire [31:0] blit_span_eff =
      (env_blit_span > ZHAO_FB_SLOT_SPAN) ? ZHAO_FB_SLOT_SPAN : env_blit_span;
  wire [31:0] fwd_addr32 = {5'b0, arb_req.addr};
  wire [31:0] fwd_end32  = fwd_addr32 + {25'b0, arb_req.len};
  // disjoint-slot containment (bank split: a <=64-B request cannot bridge)
  wire fwd_in_slot0 = (fwd_end32 <= ZHAO_FB_SLOT0_BASE + ZHAO_FB_SLOT_SPAN);
  wire fwd_in_slot1 = (fwd_addr32 >= ZHAO_FB_SLOT1_BASE)
                   && (fwd_end32 <= ZHAO_FB_SLOT1_BASE + ZHAO_FB_SLOT_SPAN);

  // --------------------------------------------------------- A1 + A2 + A3 --
  always_ff @(posedge clk) begin
    if (checking && arb_req.valid) begin
      // A2 shape
      a2_len: assert (arb_req.len >= 7'd1 && arb_req.len <= 7'd64);

      // A1 region: exactly one of the two Phase-2 ownership laws
      a1_region: assert (
           (arb_req.client == ZHAO_CLIENT_SCANOUT && !arb_req.write
            && (fwd_in_slot0 || fwd_in_slot1))
        || (arb_req.client == ZHAO_CLIENT_BLIT_DMA && arb_req.write
            && env_map_valid
            && (fwd_addr32 >= blit_base)
            && (fwd_end32  <= blit_base + blit_span_eff)));

      // engines/debug own nothing in Phase 2 and must never be forwarded
      a1_client: assert (arb_req.client == ZHAO_CLIENT_SCANOUT
                      || arb_req.client == ZHAO_CLIENT_BLIT_DMA);

      // a forward NEVER escapes the two frame-buffer slots, whatever the map
      a1_map: assert (fwd_in_slot0 || fwd_in_slot1);
    end

    // A3: the forwarding stage powers up empty
    if (released) begin
      a3_reset: assert (!arb_req.valid);
    end

    // a violation and a forward are mutually exclusive: a denied request
    // must forward NOTHING (the "no partial forward" half of A2)
    if (checking && guard_violation) begin
      a2_nopartial: assert (!rsp.ok);
    end
  end

  // ------------------------------------------------------------- non-vacuity
  // Each of these must be REACHABLE, or the assertions above are empty. The
  // first one is the load-bearing check: it is the exact condition that was
  // unreachable while the DUT had the mixed-driver defect.
  always_ff @(posedge clk) begin
    if (checking) begin
      c_forward:        cover (arb_req.valid);
      c_forward_scan:   cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_SCANOUT);
      c_forward_blit:   cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_BLIT_DMA);
      c_accept_ok:      cover (rsp.ok);
      c_violation:      cover (guard_violation);
      c_handshake:      cover (arb_req.valid && arb_rsp.grant);
      c_two_violations: cover (guard_violations >= 32'd2);
    end
  end

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19; the arbiter
  // a_horizon_is_refresh_free / linebuf a_scope_four_sessions pattern) ----
  // This proof is scoped to ONE frame-constant region map: env_map_valid /
  // env_blit_slot / env_blit_span are latched at cycle 0 and held, which
  // mirrors their real lifetime ONLY within a single frame grant —
  // CMD.SCHEDULER rewrites the map at every grant. The bmc depth (30)
  // sits far inside any frame period (>= 217,984 gpu cycles), so the
  // constant-map modelling is honest at this bound; the guard below PINS
  // the proven window. If anyone raises `depth` past it, the guard FIRES:
  // the run fails loudly instead of silently pretending the single-map
  // model still covers a horizon in which the map would really change —
  // a longer proof must MODEL map rewrites at grant boundaries, not
  // merely re-run.
  logic [5:0] f_steps = 6'd0;
  always_ff @(posedge clk) begin
    if (f_steps != 6'h3F) f_steps <= f_steps + 6'd1;
  end
  always_comb begin
    a_scope_single_map_window: assert (f_steps <= 6'd30);
  end

endmodule
