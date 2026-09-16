// zhao_renderer_lease_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-H selector shim. NOT SHIPPED. Compile
// immediately before zhao_renderer_lease_v2.sv and define exactly one selector.
`default_nettype none

`ifdef ZHAO_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: creates new renderer work while the reset-epoch barrier is closed.
    `define ZHAO_RENDERER_LEASE_ADMISSION_OPEN(open) 1'b1
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_SLOT_CHOICE
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: prefers slot 1 whenever it is FREE instead of choosing slot 0 first.
    `define ZHAO_RENDERER_LEASE_PICK_SLOT(slot0_free, slot1_free) \
      ((slot1_free) ? 1'b1 : 1'b0)
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_REQUEST_HOLD
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: a stalled manager request follows current pins/state rather than
    // the frame acceptance record.
    `define ZHAO_RENDERER_LEASE_REQ_SLOT(captured, current) (current)
    `define ZHAO_RENDERER_LEASE_REQ_MODE(captured, current) (current)
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_WRITER_RESPONSE
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: consumes the blitter's response from the shared response channel.
    `define ZHAO_RENDERER_LEASE_RESPONSE_IS_RENDERER(writer) 1'b1
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_SKIP_CLEAR
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: a renderer grant opens the frame without the V3 clear handshake.
    `define ZHAO_RENDERER_LEASE_CLEAR_REQUIRED 1'b0
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_BAD_STRIDE
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: applies Storm's row stride to Duo's 256-pixel stored surface.
    `define ZHAO_RENDERER_LEASE_DUO_STRIDE 16'd640
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_DUO_BOUNDARY
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: aliases view 1 with view 0's final RGB565 pixel.
    `define ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET 32'h0001_7FFE
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_STALE_IDENTITY
  `ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    `define ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `else
    `define ZHAO_RENDERER_LEASE_MUTANT_SELECTED
    // Wrong: the admitted frame follows the later shared response bus instead
    // of the accepted renderer grant record.
    `define ZHAO_RENDERER_LEASE_FRAME_WRITER(captured, live) (live)
    `define ZHAO_RENDERER_LEASE_FRAME_SLOT(captured, live) (live)
    `define ZHAO_RENDERER_LEASE_FRAME_GENERATION(captured, live) (live)
    `define ZHAO_RENDERER_LEASE_FRAME_MODE(captured, live) (live)
    `define ZHAO_RENDERER_LEASE_FRAME_BASE(captured, live) (live)
    `define ZHAO_RENDERER_LEASE_FRAME_SPAN(captured, live) (live)
  `endif
`endif

`ifdef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
  `undef ZHAO_RENDERER_LEASE_MUTANT_SELECTED
`endif
`default_nettype wire
