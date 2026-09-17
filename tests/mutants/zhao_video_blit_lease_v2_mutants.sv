// zhao_video_blit_lease_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-H selector shim. NOT SHIPPED. Compile
// immediately before zhao_video_blit_lease_v2.sv and define exactly one
// selector.
//
// The selectors are OBJECT-like and the function-like macros are defined here,
// which is the only shape a command-line `-D` can reach. A `-D` of a
// function-like `define` silently fails to override it and compiles production
// with no diagnostic -- see CLAUDE.md, "Verilator's -D cannot override a
// FUNCTION-LIKE define, and says nothing when it fails to".
`default_nettype none

`ifdef ZHAO_BLIT_LEASE_MUTANT_ADMISSION_BYPASS
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong: creates new blit work while the reset-epoch barrier is closed.
    `define ZHAO_BLIT_LEASE_ADMISSION_OPEN(open) 1'b1
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_WRITER_RESPONSE
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong: consumes the RENDERER's response from the shared channel. The
    // blitter then owns a slot the renderer is writing, and the renderer waits
    // forever for a response that was already retired.
    `define ZHAO_BLIT_LEASE_RESPONSE_IS_BLIT(writer) 1'b1
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_REQUEST_HOLD
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong: a stalled manager request follows the current dispatch pins
    // rather than the accepted dispatch record.
    `define ZHAO_BLIT_LEASE_REQ_SLOT(captured, current) (current)
    `define ZHAO_BLIT_LEASE_REQ_MODE(captured, current) (current)
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_LIVE_RECORD
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong: presents the live response pins as the lease record instead of
    // the frozen one. This is the metadata-swap shape from CLAUDE.md -- the
    // record tracks whatever the shared channel is OFFERING, so a later
    // response for the other writer rewrites the blitter's identity while it
    // is mid-blit.
    `define ZHAO_BLIT_LEASE_RECORD_SLOT(captured, live) (live)
    `define ZHAO_BLIT_LEASE_RECORD_GENERATION(captured, live) (live)
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_ISSUE_EARLY
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong, and it is the historical shell's own documented failure: hands
    // the request to the blitter on the edge the answer lands, so the blitter
    // latches the generation from BEFORE the grant and every publication is
    // refused as stale by a slot manager that is working perfectly.
    `define ZHAO_BLIT_LEASE_ISSUE(in_issue, in_response) \
      ((in_issue) || (in_response))
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_REFUSED_VALID
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong: presents a valid lease record for a REFUSED request, so the
    // blitter writes into a slot it does not own.
    `define ZHAO_BLIT_LEASE_RECORD_VALID(granted) 1'b1
  `endif
`endif

`ifdef ZHAO_BLIT_LEASE_MUTANT_RSP_GATED
  `ifdef ZHAO_BLIT_LEASE_MUTANT_SELECTED
    `define ZHAO_BLIT_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_BLIT_LEASE_MUTANT_SELECTED
    // Wrong, and wrong in the way that looks careful: accepts the response only
    // while the blitter is free to take the request, on the reasoning that one
    // should not hold a lease one cannot use. `rsp_*` is SHARED, and the manager
    // holds an unaccepted response -- so a busy blitter stops the RENDERER, and
    // the fault presents as a stalled renderer with nothing wrong in it.
    `define ZHAO_BLIT_LEASE_RSP_ACCEPT(in_response, blitter_ready) \
      ((in_response) && (blitter_ready))
  `endif
`endif

`default_nettype wire
