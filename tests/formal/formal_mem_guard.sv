// formal_mem_guard.sv — formal harness for mem_guard_no_escape (plan W2.5).
//
// PROPERTIES (spec/memory_rules.md §5, contract MEM.GUARD):
//   A1 no escape: whenever the guard forwards a request to the arbiter port,
//      that request lies fully inside its client's OWNED region (the Phase-2
//      map: scanout read-only within either FB slot (disjoint since the
//      W2.7 bank split); blit write-only inside the
//      CMD-granted slot window; ENGINE1 read-only inside GEOM.ASSET_POOL;
//      TERRAIN.BUILD -- and NO other client -- inside TERRAIN.PAGE_POOL, in
//      EITHER direction: WRITE for TERRAIN.PAGELOADER's pages, READ for
//      TERRAIN.WRITEBACK's layer-F sheets).
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
  input logic [31:0] env_blit_span_i,
  // Which writer the lease names. Free, and held constant for the trace like
  // the rest of the map, because a lease that changed mid-frame is a different
  // property from the one this file proves.
  input logic        env_fb_writer_i
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
  logic        env_fb_writer;
  always_ff @(posedge clk) begin
    if (cyc == 4'd0) begin
      env_map_valid <= env_map_valid_i;
      env_blit_slot <= env_blit_slot_i;
      env_blit_span <= env_blit_span_i;
      env_fb_writer <= env_fb_writer_i;
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
    .blit_span (env_blit_span), .fb_writer (env_fb_writer),
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
  // Phase-3 asset pool (spec/memory_rules.md 5f): constant bounds, read-only.
  wire fwd_in_asset = (fwd_addr32 >= ZHAO_GEOM_ASSET_BASE)
                   && (fwd_end32 <= ZHAO_GEOM_ASSET_BASE + ZHAO_GEOM_ASSET_SPAN);
  // TERRAIN.PAGE_POOL (rulings T2 / T3 / T4, spec/memory_rules.md 5b):
  // constant bounds, TERRAIN.BUILD's alone, BOTH DIRECTIONS. It is the only
  // window in the map that carries traffic each way, which is why the
  // direction statements below are split PER DIRECTION rather than folded into
  // a1_region and trusted to the spelling of pass_ok.
  wire fwd_in_terrain = (fwd_addr32 >= ZHAO_TERRAIN_PAGE_POOL_BASE)
                     && (fwd_end32  <= ZHAO_TERRAIN_PAGE_POOL_BASE
                                       + ZHAO_TERRAIN_PAGE_POOL_SPAN);

  // --------------------------------------------------------- A1 + A2 + A3 --
  always_ff @(posedge clk) begin
    if (checking && arb_req.valid) begin
      // A2 shape
      a2_len: assert (arb_req.len >= 7'd1 && arb_req.len <= 7'd64);

      // A1 region: exactly one of the FIVE ownership laws -- scanout, the two
      // framebuffer writers, ENGINE1's asset pool and TERRAIN_BUILD's page
      // pool. (This line said "two", then "three", then both at once, because
      // each pass added an arm and left the old count above it. It is a count
      // of the arms directly below; if they do not match, the comment is the
      // thing that is wrong.) ENGINE0 is
      // RASTER.FBWRITE and it is held to the SAME window as the blit, byte for
      // byte -- write-only, lease-gated, inside the clamped slot span -- so the
      // no-escape guarantee this file exists for is unchanged in MEANING: a
      // third client was admitted to an EXISTING window, not a third window
      // opened. What separates the two writers is `fb_writer`, and each is
      // required to hold the lease.
      a1_region: assert (
           (arb_req.client == ZHAO_CLIENT_SCANOUT && !arb_req.write
            && (fwd_in_slot0 || fwd_in_slot1))
        || (arb_req.client == ZHAO_CLIENT_BLIT_DMA && arb_req.write
            && env_map_valid && !env_fb_writer
            && (fwd_addr32 >= blit_base)
            && (fwd_end32  <= blit_base + blit_span_eff))
        || (arb_req.client == ZHAO_CLIENT_ENGINE0 && arb_req.write
            && env_map_valid && env_fb_writer
            && (fwd_addr32 >= blit_base)
            && (fwd_end32  <= blit_base + blit_span_eff))
        // ENGINE1 owns the Phase-3 asset pool, READ-ONLY. This arm is a
        // genuinely NEW WINDOW, not a second client admitted to an existing
        // one, and the difference is stated rather than smuggled: what keeps
        // the theorem's meaning is `!arb_req.write` -- a forward into this
        // region can never alter a frame buffer -- plus constant bounds, so no
        // map input can move it and BASE+SPAN cannot wrap.
        || (arb_req.client == ZHAO_CLIENT_ENGINE1 && !arb_req.write
            && fwd_in_asset)
        // TERRAIN.BUILD owns TERRAIN.PAGE_POOL in BOTH DIRECTIONS. A fourth
        // window, and named as one. The direction term that used to sit here
        // (`arb_req.write`) is GONE, and that is the whole amendment: ruling
        // T4 REQUIRES layer F to be evacuated on dirty eviction, and ruling T2
        // puts layer F INSIDE the page, so the sheet can be reached only by a
        // READ of this pool by this same client. What is load-bearing here
        // instead is the CLIENT term -- the region belongs to TERRAIN_BUILD and
        // to nobody else, whichever way the bytes move -- plus constant bounds,
        // so no map input can move this window and BASE + SPAN (0x054E_0000)
        // cannot wrap. The direction is still stated as a theorem, twice, at
        // a1_terrain_wr_owner / a1_terrain_rd_owner below.
        || (arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
            && fwd_in_terrain));

      // DEBUG still owns nothing and must never be forwarded, and neither
      // does the client id ruling T3 leaves unspent
      a1_client: assert (arb_req.client == ZHAO_CLIENT_SCANOUT
                      || arb_req.client == ZHAO_CLIENT_BLIT_DMA
                      || arb_req.client == ZHAO_CLIENT_ENGINE0
                      || arb_req.client == ZHAO_CLIENT_ENGINE1
                      || arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD);

      // A forward NEVER escapes THE MAP, whatever the map inputs say. The map
      // has FOUR regions now -- two framebuffer slots, the asset pool and the
      // terrain page pool -- and this line widened each time one was added,
      // which is the honest form. The alternative, leaving the old assertion
      // and exempting the new client from it, keeps a proof green by removing
      // the new region from its scope.
      a1_map: assert (fwd_in_slot0 || fwd_in_slot1 || fwd_in_asset
                   || fwd_in_terrain);

      // The asset pool is read-only at the level of the THEOREM, not merely as
      // a consequence of pass_ok's spelling: no forward into it is ever a write.
      a1_asset_ro: assert (!(fwd_in_asset && arb_req.write));

      // WHAT `a1_terrain_wo` BECAME, AND WHY IT IS REPLACED RATHER THAN
      // DELETED.
      //
      // It used to read `assert (!(fwd_in_terrain && !arb_req.write))` -- no
      // forward into the pool is ever a read. That was true of the MACHINE and
      // it was never a statement about the REGION: rulings T3 and T4 named
      // F-sheet writeback as TERRAIN_BUILD traffic from the start, and T2 put
      // layer F inside the page, so a read of this pool was ruled before it was
      // buildable and merely had no block to make it. The write-only theorem
      // was therefore a statement about WHICH BLOCKS EXISTED, and
      // `zhao_terrain_writeback.sv` is now the block. Keeping the old line
      // would force a choice between a red proof and a guard the writeback
      // cannot use.
      //
      // Deleting it outright would leave the region one theorem poorer, so it
      // is SPLIT instead, along the axis that survives the amendment:
      // OWNERSHIP, stated once per direction. Each is implied by
      // a1_terrain_owner, and that redundancy is the point -- a regression
      // names the half that broke. A stray WRITE from another client is the
      // failure this pool always had; a stray READ from another client is the
      // one the amendment makes newly possible, and it gets its own line so it
      // can never be the unnamed half of a conjunction.
      //
      // WHAT IS NOT WEAKENED. a1_map is untouched: the bounds did not move, so
      // a read arm accidentally spelled with different constants still escapes
      // the map and still fails there. a1_asset_ro is untouched. a1_client is
      // untouched. And a read cannot alter a frame buffer -- the GEOM.ASSET_POOL
      // argument -- which holds twice here, since a forwarded read carries no
      // write data and this window is disjoint from both FB slots.
      a1_terrain_wr_owner: assert (!(fwd_in_terrain && arb_req.write
                                     && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_terrain_rd_owner: assert (!(fwd_in_terrain && !arb_req.write
                                     && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_terrain_owner: assert (!(fwd_in_terrain
                                  && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
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
      // Without this the ENGINE0 arm of a1_region could be vacuous, which is
      // the exact failure this file's header records having shipped once.
      c_forward_engine: cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE0);
      // Same reason, one region later: without this the ENGINE1 arm of
      // a1_region and the whole of a1_asset_ro could be vacuously true.
      c_forward_asset:  cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE1
                               && fwd_in_asset);
      // Same reason, one region later. Without this the TERRAIN.BUILD arm of
      // a1_region and the terrain theorems above could be vacuously true --
      // and a vacuous no-escape proof is exactly what this harness's header
      // records having shipped once.
      c_forward_terrain: cover (arb_req.valid
                                && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                && fwd_in_terrain);
      // ONE COVER IS NO LONGER ENOUGH FOR THIS REGION, and that is a direct
      // consequence of the read arm. `c_forward_terrain` says only that SOME
      // forward into the pool exists; with both directions legal it can be
      // discharged by either one, so it would keep reading green while an
      // entire arm of the DUT was unreachable -- the broken-instrument failure,
      // in the file whose header already records shipping a vacuous proof once.
      // Each direction therefore carries its own.
      //
      // c_forward_terrain_wr is the OLD guarantee, now pinned to its direction:
      // TERRAIN.PAGELOADER's page deposit must still be reachable, so the
      // amendment cannot have quietly cost the write arm.
      c_forward_terrain_wr: cover (arb_req.valid
                                   && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                   && arb_req.write && fwd_in_terrain);
      // c_forward_terrain_rd is the NEW one, and it is the load-bearing half of
      // this amendment: without it, a1_terrain_rd_owner and the widened
      // a1_region arm would both hold trivially if `terrain_rd_ok` were dead
      // logic, and the proof would report PASS for a guard that still refuses
      // every sheet read. Reaching it is what makes the pass mean anything.
      c_forward_terrain_rd: cover (arb_req.valid
                                   && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                   && !arb_req.write && fwd_in_terrain);
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
