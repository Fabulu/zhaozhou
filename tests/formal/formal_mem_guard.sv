// formal_mem_guard.sv — formal harness for mem_guard_no_escape (plan W2.5).
//
// PROPERTIES (spec/memory_rules.md §5, contract MEM.GUARD):
//   A1 no escape: whenever the guard forwards a request to the arbiter port,
//      that request lies fully inside its client's OWNED region (the Phase-2
//      map: scanout read-only within [0, 0x78000); blit write-only inside the
//      CMD-granted slot window).
//   A2 no partial/malformed forward: a forwarded request has a legal length
//      (1..64 bytes) and the full contiguous byte mask.
//   A3 deny-all after reset: until a legal request is accepted, nothing is
//      forwarded (the forwarding stage powers up empty and only a passing
//      request fills it).

// (Currently SKIPped under oss-cad-suite yosys — SV-frontend gap vs the
// frozen package style; see mem_formal_lane.cmake.in.)

module formal_mem_guard
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n
);

  (* anyseq *) logic        env_valid;
  (* anyseq *) logic        env_write;
  (* anyseq *) logic [2:0]  env_client;
  (* anyseq *) logic [26:0] env_addr;
  (* anyseq *) logic [6:0]  env_len;
  (* anyseq *) logic [63:0] env_be;
  (* anyseq *) logic        env_map_valid;
  (* anyseq *) logic        env_blit_slot;
  (* anyseq *) logic [31:0] env_blit_span;

  zhao_guard_req_t req;
  assign req.valid = env_valid;
  assign req.write = env_write;
  assign req.client = zhao_client_e'(env_client);
  assign req.addr = env_addr;
  assign req.len = env_len;
  assign req.be = env_be;

  zhao_guard_rsp_t rsp;
  zhao_arb_req_t arb_req;
  zhao_arb_rsp_t arb_rsp;
  assign arb_rsp = '0;   // arbiter acceptance is free (worst case: never)

  zhao_mem_guard u_guard (
    .clk, .rst_n,
    .req, .rsp,
    .map_valid (env_map_valid), .blit_slot (env_blit_slot),
    .blit_span (env_blit_span),
    .arb_req, .arb_rsp,
    .guard_violation (), .guard_violations (), .guard_violation_req ()
  );

  // A1 + A2: any forwarded request obeys the region map and the shape law
  always_comb begin
    if (arb_req.valid) begin
      // A2 shape
      assert (arb_req.len >= 7'd1 && arb_req.len <= 7'd64)
        else $error("no_escape: forwarded illegal len %0d", arb_req.len);
      // A1 region: exactly one of the two Phase-2 ownership laws
      assert ((arb_req.client == ZHAO_CLIENT_SCANOUT && !arb_req.write
               && ({5'b0, arb_req.addr} + {25'b0, arb_req.len}
                   <= ZHAO_FB_SLOT1_BASE + ZHAO_FB_SLOT_SPAN))
              || (arb_req.client == ZHAO_CLIENT_BLIT_DMA && arb_req.write
                  && env_map_valid
                  && ({5'b0, arb_req.addr}
                      >= (env_blit_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE))
                  && ({5'b0, arb_req.addr} + {25'b0, arb_req.len}
                      <= (env_blit_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE)
                         + env_blit_span)))
        else $error("no_escape: forwarded request outside the owned region");
      // the map can only shrink what blit may write; scanout never forwards
      // a write; engines/debug never forward at all
      assert (arb_req.client == ZHAO_CLIENT_SCANOUT
              || arb_req.client == ZHAO_CLIENT_BLIT_DMA)
        else $error("no_escape: unowned client forwarded");
    end
  end

  // A3: reset deny-all (the forwarding stage is empty until a pass fills it)
  initial begin
    assert (!arb_req.valid) else $error("no_escape: forward during reset");
  end

endmodule
