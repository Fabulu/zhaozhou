// zhao_forge_cliff_srvshare.sv -- a SECOND reader for a one-clock, untagged,
// un-handshaked terrain read service, so FORGE.CLIFF can read the canonical
// lattice without taking a cycle away from the incumbent.
//
// ===========================================================================
// DECISION RECORD -- ONE PARAMETERISED SHARER, NOT TWO TYPED ONES
// ===========================================================================
// QUESTION. FORGE.CLIFF's producer needs TWO of `zhao_terrain_compcache_front`'s
// read services: the cell-state plane (`cs_req_i`/`cs_ci_i`/`cs_cj_i` ->
// `cs_substance_o`, 10 bits of request and 2 of response) for occupancy, and
// the lattice plane (`lat_req_i`/`lat_vi_i`/`lat_vj_i`/`lat_surface_i` ->
// `lat_h_o`/`lat_wx_o`/`lat_wz_o`, 13 bits of request and 96 of response) for
// the wall vertices. Both are already held by TERRAIN.TESS through
// `zhao_terrain_heighttap`. Two sharers, or one?
//
// CHOSEN. ONE block with an OPAQUE payload -- `REQ_W` bits out, `RSP_W` bits
// in -- instantiated twice. It never looks inside either payload; the console
// packs and unpacks at the instance.
//
// REASON. The two services are IDENTICAL IN THE ONLY PROPERTY THIS BLOCK CARES
// ABOUT: one clock of latency, no tag on the response, no back channel. The
// sharing law is a statement about TIMING, and the field layout is not part of
// it. Two typed copies would be two places for that law to be repaired, which
// is the duplicate-implementation failure this repository has paid for
// repeatedly.
//
// ALTERNATIVE REJECTED. Reusing `zhao_terrain_tapshare`. It is the right IDEA
// -- it is the existing N-client sharer and it was read before this was written
// -- but it is typed all the way through to the world-(x, z) service it serves
// (`r_x_i`/`r_z_i`/`r_surface_i` -> `t_req_x_o`/`t_req_z_o`, and a response with
// a `t_rsp_valid_i` QUALIFIER). The services here have no response valid at all,
// so the borrowed-cycle bookkeeping is genuinely different and bending tapshare
// to cover both would change a block TERRAIN.TESS and FORGE.SHADOW depend on.
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT AN ARBITER
// ---------------------------------------------------------------------------
// A service with no back channel CANNOT be arbitrated, because a denied client
// has nowhere to be told. So the incumbent (`o0_*`) is passed STRAIGHT THROUGH,
// every cycle, unconditionally -- its request path contains no logic that
// depends on the new client at all, which is the statement "TERRAIN.TESS cannot
// be slowed by one clock by this block existing", made structurally rather than
// argued. The new client spends the cycles the incumbent leaves idle and is told
// on the SAME cycle, by `c1_grant_o`, whether its beat was taken.
//
// That is the shape `zhao_terrain_heighttap` already uses internally for its own
// borrowed tap (`cs_go_c = cs_want_c && !o_cs_req_i`,
// fpga/rtl/terrain/zhao_terrain_heighttap.sv:351), reused deliberately.
//
// ---------------------------------------------------------------------------
// THE RESPONSE ROUTE, AND WHAT CLOCKS EACH SIDE OF IT
// ---------------------------------------------------------------------------
// The served port's answer carries no tag, so the route home is a one-deep
// memory of who won: `inflight1_q`. At most one client is issued per cycle and
// the latency is exactly one, so `inflight1_q` IS the routing tag and it is
// loaded by the same handshake that issued the request. THERE IS NO SECOND
// COUNTER IN THIS BLOCK, so there is no pair of quantities that could drift
// apart under a stall.
//
// CLAUDE.md's law -- "a detector wired to two operands that move together
// cannot fire" -- was applied before this block was written, and it removed one
// counter and kept one:
//
//   * REJECTED, never written: a "stray response" counter, `a response arrived
//     while nothing was in flight`. The serve block gates its answer on
//     `cs_req_ok_q`, which is loaded from OUR `srv_req_o`. The two sides of
//     that comparison are the same net one clock apart. It could never fire and
//     would have read zero forever as reassurance.
//
//   * KEPT: `poison1_o`. The console drives `poison_value_i` with the serve
//     block's own refusal encoding (`2'd3` for cell state), and this counts that
//     value coming back to client 1 while client 1's request WAS in flight. At
//     the composed LAT_W = LAT_H = 33 every address a client can express is in
//     range, so the only thing that produces the refusal value is
//     `serve_valid_q` LOW -- the staged patch being released or swapped
//     underneath the reader. `serve_valid_q` is clocked by the compose cache's
//     fill/release FSM; `inflight1_q` is clocked by this block's issue.
//     DIFFERENT ENABLES, so the comparison can see a TIMING fault and not merely
//     a value fault, which is the whole point. It is reachable with legal
//     stimulus -- drop `serve_valid` mid-page -- and the bench does that.
//
// `POISON_EN` exists because the lattice service has no refusal encoding to
// watch: `lat_h_o`/`lat_wx_o`/`lat_wz_o` are raw fx16 and EVERY bit pattern is
// a legal height. A detector there would have to invent a sentinel, which would
// be a detector watching a value this block made up. Set POISON_EN = 0 and the
// counter is tied low and says so, rather than reading zero as if it had looked.
//
// Conservative SystemVerilog subset (charter 2): elaboration checks inside
// `initial begin`, no bare module-scope `if`, explicit generate/endgenerate.

module zhao_forge_cliff_srvshare #(
    parameter int unsigned REQ_W    = 10,
    parameter int unsigned RSP_W    = 2,
    // 1 = watch for the service's refusal encoding coming back in flight.
    // 0 = the service has no refusal encoding; `poison1_o` is tied low.
    parameter bit          POISON_EN = 1'b1,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // The serve block's refusal encoding, in the response's own bit layout.
    // Ignored entirely when POISON_EN = 0.
    input var logic [RSP_W-1:0] poison_value_i,

    // ---- client 0: THE INCUMBENT. Never denied, never delayed. --------------
    input  var logic             o0_req_i,
    input  var logic [REQ_W-1:0] o0_req_payload_i,
    output var logic [RSP_W-1:0] o0_rsp_payload_o,

    // ---- client 1: FORGE.CLIFF -----------------------------------------------
    input  var logic             c1_req_i,
    input  var logic [REQ_W-1:0] c1_req_payload_i,
    output var logic             c1_grant_o,      // combinational, this cycle
    output var logic             c1_rsp_valid_o,  // registered, one clock later
    output var logic [RSP_W-1:0] c1_rsp_payload_o,

    // ---- the served port ------------------------------------------------------
    output var logic             srv_req_o,
    output var logic [REQ_W-1:0] srv_req_payload_o,
    input  var logic [RSP_W-1:0] srv_rsp_payload_i,

    // ---- evidence -------------------------------------------------------------
    output var logic [CENSUS_W-1:0] grants0_o,  // cycles the incumbent asked
    output var logic [CENSUS_W-1:0] grants1_o,  // cycles client 1 was issued
    output var logic [CENSUS_W-1:0] denied1_o,  // cycles client 1 asked and lost
    output var logic [CENSUS_W-1:0] poison1_o   // FAULT: refusal answered in flight
);

  initial begin
    if (REQ_W < 1 || RSP_W < 1) begin
      $fatal(1, "zhao_forge_cliff_srvshare: REQ_W/RSP_W must be >= 1 (%0d/%0d)", REQ_W, RSP_W);
    end
    if (CENSUS_W < 8) begin
      $fatal(1, "zhao_forge_cliff_srvshare: CENSUS_W must be >= 8 (is %0d)", CENSUS_W);
    end
  end

  // ---- issue ---------------------------------------------------------------
  logic grant1_c;
  assign grant1_c = c1_req_i && !o0_req_i;

  assign srv_req_o         = o0_req_i || grant1_c;
  assign srv_req_payload_o = o0_req_i ? o0_req_payload_i : c1_req_payload_i;

  assign c1_grant_o = grant1_c;

  // ---- the one-deep routing tag --------------------------------------------
  logic inflight1_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      inflight1_q <= 1'b0;
    end else begin
      inflight1_q <= grant1_c;
    end
  end

  // The incumbent's answer is the served answer, always. Splicing this block in
  // is therefore transparent to it.
  assign o0_rsp_payload_o = srv_rsp_payload_i;

  assign c1_rsp_valid_o   = inflight1_q;
  assign c1_rsp_payload_o = srv_rsp_payload_i;

  // ---- evidence -------------------------------------------------------------
  logic [CENSUS_W-1:0] grants0_r, grants1_r, denied1_r, poison1_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      grants0_r <= '0;
      grants1_r <= '0;
      denied1_r <= '0;
      poison1_r <= '0;
    end else begin
      if (o0_req_i && !(&grants0_r)) begin
        grants0_r <= grants0_r + CENSUS_W'(1);
      end
      if (grant1_c && !(&grants1_r)) begin
        grants1_r <= grants1_r + CENSUS_W'(1);
      end
      if (c1_req_i && o0_req_i && !(&denied1_r)) begin
        denied1_r <= denied1_r + CENSUS_W'(1);
      end
      if (POISON_EN && inflight1_q && (srv_rsp_payload_i == poison_value_i) &&
          !(&poison1_r)) begin
        poison1_r <= poison1_r + CENSUS_W'(1);
      end
    end
  end

  assign grants0_o = grants0_r;
  assign grants1_o = grants1_r;
  assign denied1_o = denied1_r;
  assign poison1_o = poison1_r;

endmodule
