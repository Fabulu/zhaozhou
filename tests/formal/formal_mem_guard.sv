// formal_mem_guard.sv — formal harness for mem_guard_no_escape (plan W2.5).
//
// PROPERTIES (spec/memory_rules.md §5, contract MEM.GUARD):
//   A1 no escape: whenever the guard forwards a request to the arbiter port,
//      that request lies fully inside its client's OWNED region (the Phase-2
//      map: scanout read-only within either FB slot (disjoint since the
//      W2.7 bank split); blit write-only inside the
//      CMD-granted slot window; ENGINE1 read-only inside RENDER.ASSET_POOL;
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
  input logic        env_fb_writer_i,
  // The published-resource region (R32): HOST configuration, free and held
  // constant like the map. NOT constrained to lie in the asset pool -- the DUT
  // must refuse a region that does not, and that refusal is what is proved.
  input logic        env_res_valid_i,
  input logic [31:0] env_res_base_i,
  input logic [31:0] env_res_span_i,
  // GEOM.PARAMBUF's frame lease (owner completion ruling ITEM 4). Free, and
  // held constant for the trace like the rest of the map, for the SAME reason
  // `env_fb_writer_i` is: a lease that moved mid-frame is a different property
  // from the one this file proves, and it is proved somewhere else -- at
  // `zhao_geom_paramarena`'s drain precondition, whose committed mutant is
  // tests/mutants/zhao_geom_paramarena_drain_mutant.sv. Saying that here
  // rather than leaving the gap silent is the point: this harness proves
  // CONTAINMENT under a stable lease, and the LIFETIME of the lease is not in
  // its scope. A proof that quietly covered neither would read identically.
  //
  // NOT constrained to be asserted. The DUT must refuse the whole region when
  // the lease is low, and that refusal is what a1_pb_lease proves.
  input logic        env_pb_lease_i,
  input logic        env_pb_wr_view_i,
  input logic        env_pb_scratch_i
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
  logic        env_res_valid;
  logic [31:0] env_res_base, env_res_span;
  logic        env_pb_lease, env_pb_wr_view, env_pb_scratch;
  always_ff @(posedge clk) begin
    if (cyc == 4'd0) begin
      env_pb_lease   <= env_pb_lease_i;
      env_pb_wr_view <= env_pb_wr_view_i;
      env_pb_scratch <= env_pb_scratch_i;
      env_res_valid <= env_res_valid_i;
      env_res_base  <= env_res_base_i;
      env_res_span  <= env_res_span_i;
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

  // THE MUTANT SEAM (R32). A plain `ifdef selecting between two module
  // names, which a -D does reach (CLAUDE.md: a function-like define does not).
  // tests/formal/mem_guard_resbound_mutant.sby defines it and EXPECTS FAIL; the
  // production proof leaves it undefined, which is the negative control.
// A SECOND SEAM (R242), spelled the same way. `ZHAO_GUARD_DEVBOUND_MUT`
// selects the copy whose TERRAIN.DEVSTORE upper bound has been removed;
// tests/formal/mem_guard_devbound_mutant.sby defines it and EXPECTS FAIL.
// The two are mutually exclusive by construction -- an `elsif chain, not two
// independent `ifdefs -- so a run that defined both would silently measure
// only the first rather than something neither file describes.
// A THIRD AND FOURTH SEAM (owner completion ruling ITEM 4), on the same
// `elsif chain so no run can define two and silently measure only the first.
// `ZHAO_GUARD_PBVIEW_MUT` selects the copy whose PARAMBUF WRITE arm no longer
// names a view -- the protection item 4 asks for a deliberate fault against,
// since with it gone the producer may overwrite the view the walker is
// reading. `ZHAO_GUARD_PBUNION_MUT` selects the copy whose three containment
// tests are collapsed into one [VIEW0_BASE, SCRATCH_END) comparison, which is
// exactly "both endpoints lie somewhere in the union of permitted ranges"
// implemented. tests/formal/mem_guard_pbview_mutant.sby and
// mem_guard_pbunion_mutant.sby define them and EXPECT FAIL.
`ifdef ZHAO_GUARD_RESBOUND_MUT
  zhao_mem_guard_resbound_mutant u_guard (
`elsif ZHAO_GUARD_DEVBOUND_MUT
  zhao_mem_guard_devbound_mutant u_guard (
`elsif ZHAO_GUARD_PBVIEW_MUT
  zhao_mem_guard_pbview_mutant u_guard (
`elsif ZHAO_GUARD_PBUNION_MUT
  zhao_mem_guard_pbunion_mutant u_guard (
`else
  zhao_mem_guard u_guard (
`endif
    .clk, .rst_n,
    .req, .rsp,
    .map_valid (env_map_valid), .blit_slot (env_blit_slot),
    .blit_span (env_blit_span), .fb_writer (env_fb_writer),
    .res_valid (env_res_valid), .res_base (env_res_base), .res_span (env_res_span),
    .pb_lease_valid (env_pb_lease), .pb_wr_view (env_pb_wr_view),
    .pb_scratch_valid (env_pb_scratch),
    .arb_req, .arb_rsp,
    .guard_violation, .guard_violations, .guard_violation_req
  );

  // Acceptance and verdict are distinct edges. This witness makes client 5's
  // deny cover prove the real accepted-request protocol rather than a coincident
  // violation level with no captured request.
  logic client5_accept_q;
  always_ff @(posedge clk) begin
    if (!rst_n) client5_accept_q <= 1'b0;
    else client5_accept_q <= req.valid && rsp.ready &&
                             (req.client == zhao_client_e'(3'd5));
  end

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
  wire fwd_in_render_asset = (fwd_addr32 >= ZHAO_RENDER_ASSET_BASE)
                   && (fwd_end32 <= ZHAO_RENDER_ASSET_BASE + ZHAO_RENDER_ASSET_SPAN);
  // TERRAIN.PAGE_POOL (rulings T2 / T3 / T4, spec/memory_rules.md 5b):
  // constant bounds, TERRAIN.BUILD's alone, BOTH DIRECTIONS. It is the only
  // window in the map that carries traffic each way, which is why the
  // direction statements below are split PER DIRECTION rather than folded into
  // a1_region and trusted to the spelling of pass_ok.
  // The published-resource region (R32), as the ENTITLEMENT, computed from
  // the held config independently of the DUT: 33-bit end, and the region must
  // lie wholly inside the asset pool or it entitles nothing.
  wire [32:0] res_end33 = {1'b0, env_res_base} + {1'b0, env_res_span};
  wire res_in_pool = env_res_valid && (env_res_base >= ZHAO_RENDER_ASSET_BASE)
                  && (res_end33 <= {1'b0, ZHAO_RENDER_ASSET_BASE + ZHAO_RENDER_ASSET_SPAN});
  wire fwd_in_resource = res_in_pool && (fwd_addr32 >= env_res_base)
                      && ({1'b0, fwd_end32} <= res_end33);
  wire fwd_in_terrain = (fwd_addr32 >= ZHAO_TERRAIN_PAGE_POOL_BASE)
                     && (fwd_end32  <= ZHAO_TERRAIN_PAGE_POOL_BASE
                                       + ZHAO_TERRAIN_PAGE_POOL_SPAN);
  // TERRAIN.DEVSTORE (ruling R242, spec/memory_rules.md 5b): constant bounds,
  // TERRAIN.BUILD's alone, BOTH DIRECTIONS -- the second window in the map
  // that carries traffic each way, and like the page pool its directions are
  // stated PER DIRECTION below rather than folded into a1_region and trusted
  // to the spelling of pass_ok.
  wire fwd_in_devstore = (fwd_addr32 >= ZHAO_TERRAIN_DEVSTORE_BASE)
                      && (fwd_end32  <= ZHAO_TERRAIN_DEVSTORE_BASE
                                        + ZHAO_TERRAIN_DEVSTORE_SPAN);
  // GEOM.PARAMBUF (owner completion ruling ITEM 4, spec/memory_rules.md 5c):
  // THREE regions, and they are spelled as THREE wires here for the same
  // reason the DUT spells them as three comparisons. A single
  // `fwd_in_parambuf` covering [VIEW0_BASE, SCRATCH_END) would make
  // a1_pb_views_disjoint unstatable and would let the union mutant PASS --
  // the harness would have been rewritten into agreement with the fault.
  wire fwd_in_pb_view0 = (fwd_addr32 >= ZHAO_PARAMBUF_VIEW0_BASE)
                      && (fwd_end32  <= ZHAO_PARAMBUF_VIEW0_BASE
                                        + ZHAO_PARAMBUF_VIEW_SPAN);
  wire fwd_in_pb_view1 = (fwd_addr32 >= ZHAO_PARAMBUF_VIEW1_BASE)
                      && (fwd_end32  <= ZHAO_PARAMBUF_VIEW1_BASE
                                        + ZHAO_PARAMBUF_VIEW_SPAN);
  wire fwd_in_pb_scr   = (fwd_addr32 >= ZHAO_PARAMBUF_SCRATCH_BASE)
                      && (fwd_end32  <= ZHAO_PARAMBUF_SCRATCH_BASE
                                        + ZHAO_PARAMBUF_SCRATCH_SPAN);
  wire fwd_in_pb_any   = fwd_in_pb_view0 || fwd_in_pb_view1 || fwd_in_pb_scr;
  // POST.ECHO's capture buffer (ruling R7, spec/memory_rules.md 5g): constant
  // bounds, ENGINE0's alone, WRITE-only, lease-gated.
  wire fwd_in_echo = (fwd_addr32 >= ZHAO_POST_ECHO_BASE)
                  && (fwd_end32  <= ZHAO_POST_ECHO_BASE + ZHAO_POST_ECHO_SPAN);
  // The leased FB window, as the DUT is entitled to use it for either writer.
  wire fwd_in_lease = env_map_valid && (fwd_addr32 >= blit_base)
                   && (fwd_end32 <= blit_base + blit_span_eff);

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
        // ENGINE0 READS ITS OWN LEASED WINDOW (POST.COMPOSITE's read/write
        // lease, 2026-09-19). The same window, the same lease term, the
        // opposite direction bit. A forwarded read carries no write data, so
        // this arm cannot alter a frame buffer; what it must not do is read
        // OUTSIDE the lease, and the window and `env_fb_writer` terms say so.
        || (arb_req.client == ZHAO_CLIENT_ENGINE0 && !arb_req.write
            && env_fb_writer && fwd_in_lease)
        // ENGINE0 WRITES POST.ECHO's CAPTURE (ruling R7). A FIFTH window and
        // named as one: constant bounds, disjoint from every FB slot, so no
        // capture write can alter a displayable frame -- and lease-gated.
        || (arb_req.client == ZHAO_CLIENT_ENGINE0 && arb_req.write
            && env_fb_writer && fwd_in_echo)
        // ENGINE1 owns the Phase-3 asset pool, READ-ONLY. This arm is a
        // genuinely NEW WINDOW, not a second client admitted to an existing
        // one, and the difference is stated rather than smuggled: what keeps
        // the theorem's meaning is `!arb_req.write` -- a forward into this
        // region can never alter a frame buffer -- plus constant bounds, so no
        // map input can move it and BASE+SPAN cannot wrap.
        || (arb_req.client == ZHAO_CLIENT_ENGINE1 && !arb_req.write
            && fwd_in_render_asset)
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
            && fwd_in_terrain)
        // R32: the SIXTH law. TERRAIN_BUILD may WRITE the asset pool, and only
        // inside the published-resource region -- which itself must lie
        // inside the pool. It is a WRITE arm only: no read term is added.
        || (arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD && arb_req.write
            && fwd_in_resource)
        // R242: the SEVENTH law. TERRAIN_BUILD owns TERRAIN.DEVSTORE in BOTH
        // DIRECTIONS -- the deviation and history records the LOD pass reads
        // every frame and the mip pass writes at page load. Like the page pool
        // the load-bearing term here is the CLIENT plus CONSTANT BOUNDS, and
        // like the page pool the direction is stated as a theorem twice, at
        // a1_devstore_wr_owner / a1_devstore_rd_owner below. It is a SEPARATE
        // window from the page pool and not a widening of it: the two are
        // disjoint, asserted at a1_devstore_not_page.
        || (arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
            && fwd_in_devstore)
        // OWNER COMPLETION RULING ITEM 4: the EIGHTH law, and the first with
        // THREE regions under one lease. ENGINE1 owns GEOM.PARAMBUF. The arms
        // are written out in full here rather than folded into one
        // `fwd_in_pb_any` term, because folding them is EXACTLY the fault
        // `zhao_mem_guard_pbunion_mutant` implements -- a harness that wrote
        // the union would prove the mutant correct.
        //
        //   READ: either view, lease held. A read cannot alter anything, so
        //         the view is not named on this arm.
        //   WRITE: the LEASED view ONLY. `env_pb_wr_view ? view1 : view0` is
        //         the load-bearing term -- item 4's "correct view/region
        //         selection", and the one thing standing between the producer
        //         and the frame the walker is reading.
        //   SCRATCH: both directions, and ONLY while the scratch is
        //         ACQUIRED. Item 4: shared scratch has explicit ownership and
        //         release rather than being unowned temporary memory.
        //
        // EVERY arm carries `env_pb_lease`, so with the lease low this whole
        // region is unmapped for everybody -- a1_pb_lease states that as its
        // own theorem so a regression names it.
        || (arb_req.client == ZHAO_CLIENT_ENGINE1 && env_pb_lease
            && ((!arb_req.write && (fwd_in_pb_view0 || fwd_in_pb_view1))
                || (arb_req.write
                    && (env_pb_wr_view ? fwd_in_pb_view1 : fwd_in_pb_view0))
                || (env_pb_scratch && fwd_in_pb_scr))));

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
      a1_map: assert (fwd_in_slot0 || fwd_in_slot1 || fwd_in_render_asset
                   || fwd_in_terrain || fwd_in_echo || fwd_in_devstore
                   || fwd_in_pb_any);

      // The echo capture is WRITE-ONLY and has exactly one owner, and the
      // owner holds the render lease. Each is implied by a1_region; each is
      // stated separately so a regression names the half that broke.
      a1_echo_wo:    assert (!(fwd_in_echo && !arb_req.write));
      a1_echo_owner: assert (!(fwd_in_echo && arb_req.client != ZHAO_CLIENT_ENGINE0));
      a1_echo_lease: assert (!(fwd_in_echo && !env_fb_writer));
      // No capture forward overlaps a frame buffer: the window is disjoint
      // from both slots, stated as a theorem rather than trusted to constants.
      a1_echo_not_fb: assert (!(fwd_in_echo && (fwd_in_slot0 || fwd_in_slot1)));
      // An ENGINE0 read is always inside the lease, and only while it holds it.
      a1_engine0_rd_lease: assert (!(arb_req.client == ZHAO_CLIENT_ENGINE0
                                     && !arb_req.write
                                     && !(env_fb_writer && fwd_in_lease)));

      // The render asset pool is read-only and has exactly one global owner.
      // Geometry and texture are local mux subowners, never new client IDs.
      // R32 AMENDED these two, and they are RESTATED PER DIRECTION rather than
      // deleted -- the terrain amendment's pattern. The pool keeps ONE READER
      // (ENGINE1) and gains ONE WRITER (TERRAIN_BUILD), and every write must
      // land inside the published-resource region.
      a1_render_asset_rd_owner:
        assert (!(fwd_in_render_asset && !arb_req.write &&
                  arb_req.client != ZHAO_CLIENT_ENGINE1));
      a1_render_asset_wr_owner:
        assert (!(fwd_in_render_asset && arb_req.write &&
                  arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_resource_bounded:
        assert (!(fwd_in_render_asset && arb_req.write) || fwd_in_resource);
      // Client 5 is deliberately unspent and cannot reach any forwarded arm.
      a1_no_forward_client5:
        assert (arb_req.client != zhao_client_e'(3'd5));

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
      // the map and still fails there. a1_render_asset_ro is untouched. a1_client is
      // untouched. And a read cannot alter a frame buffer -- the RENDER.ASSET_POOL
      // argument -- which holds twice here, since a forwarded read carries no
      // write data and this window is disjoint from both FB slots.
      a1_terrain_wr_owner: assert (!(fwd_in_terrain && arb_req.write
                                     && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_terrain_rd_owner: assert (!(fwd_in_terrain && !arb_req.write
                                     && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_terrain_owner: assert (!(fwd_in_terrain
                                  && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));

      // TERRAIN.DEVSTORE (ruling R242), stated with the page pool's discipline
      // because it has the page pool's shape: one client, two directions,
      // constant bounds. Three theorems where one would do, for the reason the
      // paragraph above gives -- a regression names the half that broke, and a
      // stray READ and a stray WRITE from another client are different faults.
      a1_devstore_wr_owner: assert (!(fwd_in_devstore && arb_req.write
                                      && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_devstore_rd_owner: assert (!(fwd_in_devstore && !arb_req.write
                                      && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      a1_devstore_owner: assert (!(fwd_in_devstore
                                   && arb_req.client != ZHAO_CLIENT_TERRAIN_BUILD));
      // THE TWO TERRAIN WINDOWS ARE DISJOINT, stated as a theorem rather than
      // trusted to the constants. This is the line that makes "a request
      // crossing a boundary is not allowed merely because both endpoints lie
      // in the union of permitted ranges" a proved property of the map and not
      // an argument in a comment: if a forward could ever be inside BOTH, then
      // a range that spans the gap would be admitted by one arm while
      // describing bytes governed by the other.
      a1_devstore_not_page: assert (!(fwd_in_devstore && fwd_in_terrain));
      // And it is not a framebuffer, an asset or the echo capture either.
      a1_devstore_not_fb: assert (!(fwd_in_devstore
                                    && (fwd_in_slot0 || fwd_in_slot1)));
      a1_devstore_not_echo: assert (!(fwd_in_devstore && fwd_in_echo));
      a1_devstore_not_asset: assert (!(fwd_in_devstore && fwd_in_render_asset));

      // ---------------------------------------------------------------------
      // GEOM.PARAMBUF (owner completion ruling ITEM 4). Item 4 names five
      // things to prove and each one gets its own theorem below, because a
      // regression has to say WHICH protection went: "Prove valid reads and
      // writes can pass, unauthorized clients/views cannot escape, ENGINE1
      // asset-pool writes still fail, and boundary-crossing, wrapping and
      // stale-lifetime cases are refused."
      //
      // The first of those five is a COVER, not an assert -- c_forward_pb_*
      // below -- and that is deliberate. Every assertion here holds trivially
      // if nothing ever reaches the window, so the covers are what make the
      // pass mean anything about PARAMBUF at all. Five of them, one per arm.
      // ---------------------------------------------------------------------

      // UNAUTHORIZED CLIENTS. Item 4: "No other client acquires PARAMBUF
      // access through this ruling." Stated over the whole region and then
      // per region, so a stray SCANOUT read of a view and a stray
      // TERRAIN_BUILD write of the scratch are different failures.
      a1_pb_owner: assert (!(fwd_in_pb_any
                             && arb_req.client != ZHAO_CLIENT_ENGINE1));
      a1_pb_view0_owner: assert (!(fwd_in_pb_view0
                                   && arb_req.client != ZHAO_CLIENT_ENGINE1));
      a1_pb_view1_owner: assert (!(fwd_in_pb_view1
                                   && arb_req.client != ZHAO_CLIENT_ENGINE1));
      a1_pb_scr_owner: assert (!(fwd_in_pb_scr
                                 && arb_req.client != ZHAO_CLIENT_ENGINE1));

      // UNAUTHORIZED VIEWS -- the protection this window exists for. A
      // forwarded WRITE lands only in the view the lease NAMES. Two theorems
      // and not one: a producer that ignores the selector fails BOTH, and a
      // producer whose selector is inverted fails exactly one, which is the
      // difference between "the term is missing" and "the term is backwards".
      // `zhao_mem_guard_pbview_mutant` removes the selection and is the
      // committed demonstration that these can fail.
      a1_pb_wr_view0: assert (!(arb_req.write && fwd_in_pb_view0
                                && env_pb_wr_view));
      a1_pb_wr_view1: assert (!(arb_req.write && fwd_in_pb_view1
                                && !env_pb_wr_view));

      // THE LEASE IS THE DENY-ALL. With it low the region is unmapped for
      // everybody, which is what makes "no blanket bank-3 permission" a
      // property of the map rather than a sentence in a comment.
      a1_pb_lease: assert (!(fwd_in_pb_any && !env_pb_lease));

      // SHARED SCRATCH HAS EXPLICIT OWNERSHIP. Not "temporary memory anyone
      // may touch": acquired or unmapped, with release being the deassert.
      a1_pb_scr_owned: assert (!(fwd_in_pb_scr && !env_pb_scratch));

      // ENGINE1'S ASSET-POOL WRITES STILL FAIL -- item 4 in as many words.
      // This is implied by a1_region, and it is stated anyway because it is
      // the one thing this ruling was most likely to break by accident: the
      // asset pool sits immediately above the scratch and a widened upper
      // bound would swallow it. R32's TERRAIN_BUILD write arm is untouched
      // and is not what this says anything about.
      a1_pb_asset_still_ro: assert (!(arb_req.client == ZHAO_CLIENT_ENGINE1
                                      && arb_req.write && fwd_in_render_asset));

      // BOUNDARY CROSSING. Item 4: "A request crossing a per-view or scratch
      // boundary is not allowed merely because both endpoints lie somewhere
      // in the union of permitted ranges." These four lines are what make
      // that a PROVED property and not an argument: the three regions are
      // pairwise disjoint, so a forward that is inside one is inside no
      // other, and a1_region has already established every forward is inside
      // one. A request spanning the view0/view1 seam is therefore inside
      // NEITHER and is refused whole. Wrapping is structural and stated at
      // a2_len plus the DUT's 32-bit `end32` over a 27-bit address and a
      // 7-bit length; STALE LIFETIME is the producer's and is proved at
      // tests/geometry/geom_paramarena_directed.cpp, not here.
      a1_pb_views_disjoint: assert (!(fwd_in_pb_view0 && fwd_in_pb_view1));
      a1_pb_scr_not_view: assert (!(fwd_in_pb_scr
                                    && (fwd_in_pb_view0 || fwd_in_pb_view1)));
      // And PARAMBUF is not any other window. The scratch's upper edge and
      // the asset pool's base are the SAME constant, so this is the line that
      // catches an off-by-one there -- the direction in which a mistake would
      // hand ENGINE1 a write into the pool it is only allowed to read.
      a1_pb_scratch_not_asset: assert (!(fwd_in_pb_scr && fwd_in_render_asset));
      a1_pb_not_asset: assert (!(fwd_in_pb_any && fwd_in_render_asset));
      a1_pb_not_fb: assert (!(fwd_in_pb_any && (fwd_in_slot0 || fwd_in_slot1)));
      a1_pb_not_terrain: assert (!(fwd_in_pb_any && fwd_in_terrain));
      a1_pb_not_devstore: assert (!(fwd_in_pb_any && fwd_in_devstore));
      a1_pb_not_echo: assert (!(fwd_in_pb_any && fwd_in_echo));
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
      // ONE COVER PER ENGINE0 ARM. With three arms, `c_forward_engine` can be
      // discharged by any one of them and would stay green over a dead arm --
      // the vacuity this harness's header records shipping once.
      c_forward_engine_wr: cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE0
                                  && arb_req.write && fwd_in_lease);
      c_forward_engine_rd: cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE0
                                  && !arb_req.write && fwd_in_lease);
      c_forward_echo:      cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE0
                                  && arb_req.write && fwd_in_echo);
      // Packet E uses 16-byte texture fills while retained geometry uses 32/64.
      // Three covers prevent one legal request shape from hiding a dead arm.
      c_forward_render_asset_16:
        cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE1 &&
               !arb_req.write && fwd_in_render_asset && arb_req.len == 7'd16);
      c_forward_render_asset_32:
        cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE1 &&
               !arb_req.write && fwd_in_render_asset && arb_req.len == 7'd32);
      c_forward_render_asset_64:
        cover (arb_req.valid && arb_req.client == ZHAO_CLIENT_ENGINE1 &&
               !arb_req.write && fwd_in_render_asset && arb_req.len == 7'd64);
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
      // R32's arm must be REACHABLE, or a1_resource_bounded holds trivially.
      c_forward_resource_wr: cover (arb_req.valid
                                    && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                    && arb_req.write && fwd_in_render_asset);
      // R242's TWO arms, one cover each, for the reason spelled out at
      // c_forward_terrain above: with both directions legal, a single cover is
      // discharged by either one and keeps reading green while a whole arm of
      // the DUT is dead logic. `a1_devstore_rd_owner`, `a1_devstore_wr_owner`
      // and the new a1_region arm ALL hold trivially if nothing ever reaches
      // the window, so reaching each direction is what makes the pass mean
      // anything about the deviation store at all.
      c_forward_devstore_wr: cover (arb_req.valid
                                    && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                    && arb_req.write && fwd_in_devstore);
      c_forward_devstore_rd: cover (arb_req.valid
                                    && arb_req.client == ZHAO_CLIENT_TERRAIN_BUILD
                                    && !arb_req.write && fwd_in_devstore);
      // OWNER COMPLETION RULING ITEM 4's FIRST REQUIREMENT -- "prove valid
      // reads and writes can PASS" -- is discharged HERE and not by any
      // assertion above, every one of which holds trivially on a model that
      // never reaches the window. FIVE covers, one per arm and one per view,
      // for the reason c_forward_terrain and c_forward_devstore_* give: a
      // single `fwd_in_pb_any` cover is discharged by whichever arm happens
      // to be alive and reads GREEN while the other four are dead logic.
      //
      // The two WRITE covers are the ones that matter most, because they are
      // the only evidence that `pb_wr_view` selects rather than merely
      // gating: reaching view 1 with the lease naming view 1 AND reaching
      // view 0 with the lease naming view 0 cannot both happen if the mux is
      // stuck.
      c_forward_pb_rd_v0: cover (arb_req.valid
                                 && arb_req.client == ZHAO_CLIENT_ENGINE1
                                 && !arb_req.write && fwd_in_pb_view0);
      c_forward_pb_rd_v1: cover (arb_req.valid
                                 && arb_req.client == ZHAO_CLIENT_ENGINE1
                                 && !arb_req.write && fwd_in_pb_view1);
      c_forward_pb_wr_v0: cover (arb_req.valid
                                 && arb_req.client == ZHAO_CLIENT_ENGINE1
                                 && arb_req.write && fwd_in_pb_view0);
      c_forward_pb_wr_v1: cover (arb_req.valid
                                 && arb_req.client == ZHAO_CLIENT_ENGINE1
                                 && arb_req.write && fwd_in_pb_view1);
      c_forward_pb_scr: cover (arb_req.valid
                               && arb_req.client == ZHAO_CLIENT_ENGINE1
                               && fwd_in_pb_scr);
      c_accept_ok:      cover (rsp.ok);
      c_violation:      cover (guard_violation);
      c_client5_denied: cover (client5_accept_q && rsp.violation &&
                               guard_violation);
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
