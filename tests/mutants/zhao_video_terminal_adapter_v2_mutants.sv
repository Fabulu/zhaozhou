// zhao_video_terminal_adapter_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-H selector shim. NOT SHIPPED. Compile
// immediately before zhao_video_terminal_adapter_v2.sv and define exactly one
// selector. Each mutation changes one protocol decision while fixed observations
// remain available to the independent host scoreboard.
`default_nettype none

`ifdef ZHAO_VIDEO_TERM_MUTANT_DROP_HELD
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: hides valid whenever the manager stalls.
    `define ZHAO_VIDEO_TERM_VALID(occupied, ready) ((occupied) && (ready))
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_CHANGE_HELD
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: a retained terminal follows the current renderer bus.
    `define ZHAO_VIDEO_TERM_TUPLE(held, live) (live)
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_WRONG_WRITER
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: reverses the source tag on every manager terminal.
    `define ZHAO_VIDEO_TERM_WRITER(writer) (!(writer))
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_PUBLISH_WINS
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: matching simultaneous blitter publish/release becomes publication.
    // Suppress only the production aborting assertion in this mutant build so
    // the independent host can observe and name the malformed held tuple.
    `define ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT
    `define ZHAO_VIDEO_TERM_RELEASE_CHOICE(release_valid, publish_valid, same_key) \
      ((release_valid) && !((publish_valid) && (same_key)))
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_RENDER_READY
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: tells a losing renderer that its terminal was captured.
    `define ZHAO_VIDEO_TERM_RENDER_READY(selected, room, render_valid) \
      ((room) && (render_valid))
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_SILENT_BLIT_DROP
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: erases explicit evidence that pulse-only blitter work was refused.
    `define ZHAO_VIDEO_TERM_REFUSE_DELTA(delta) 2'd0
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_NO_POP_REPLACE
  `ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
    `define ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `else
    `define ZHAO_VIDEO_TERM_MUTANT_SELECTED
    // Wrong: refuses an incoming pulse even while the old terminal is accepted.
    `define ZHAO_VIDEO_TERM_ROOM(occupied, pop) (!(occupied))
  `endif
`endif

`ifdef ZHAO_VIDEO_TERM_MUTANT_SELECTED
  `undef ZHAO_VIDEO_TERM_MUTANT_SELECTED
`endif
`default_nettype wire
