// zhao_video_ready_bridge_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-H selector shim.  NOT SHIPPED.  Compile
// immediately before zhao_video_ready_bridge_v2.sv and define exactly one
// selector.  Every mutation changes a production decision rather than a test
// observation; the directed host must pass only after observing the named fault.
`default_nettype none

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_DROP_HELD_TUPLE
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // Illegal ready-dependent valid: the echo disappears whenever CDC stalls.
    `define ZHAO_VIDEO_BRIDGE_ECHO_VALID(held, ready) ((held) && (ready))
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_CHANGE_HELD_TUPLE
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // Illegal live-input bypass: valid remains held but payload is no longer the
    // tuple captured at the FRAMECTL decision.
    `define ZHAO_VIDEO_BRIDGE_ECHO_TUPLE(held, offered) (offered)
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_WRONG_ONEHOT
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // Advertises both slots instead of the one named by the pending tuple.
    `define ZHAO_VIDEO_BRIDGE_SLOT_ONEHOT(slot) 2'b11
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_BLANK_ACK_BYPASS
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // The command leaks directly to ACK before the output register turns black.
    `define ZHAO_VIDEO_BRIDGE_BLANK_ACK(registered_ack, command) \
        ((registered_ack) || (command))
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_EARLY_LEASE_OPEN
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // FIFO barriers alone are not proof that the registered video output is black.
    `define ZHAO_VIDEO_BRIDGE_LEASE_GATE(gpu_barrier, vid_barrier, blank_ack) \
        ((gpu_barrier) && (vid_barrier))
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_RETAIN_RESET_TUPLE
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // A missing async clear retains both valid bits and their tuple payloads.
    `define ZHAO_VIDEO_BRIDGE_RESET_VALUE(current) (current)
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_UNBLANK_SCANOUT_ONLY
  `ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    `define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
    // Illegal one-sided release: scanout ACK bypasses exact echo acceptance.
    `define ZHAO_VIDEO_BRIDGE_UNBLANK_JOIN(echo_seen, scanout_seen) \
        (scanout_seen)
  `endif
`endif

`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
  `undef ZHAO_VIDEO_BRIDGE_MUTANT_SELECTED
`endif
`default_nettype wire
