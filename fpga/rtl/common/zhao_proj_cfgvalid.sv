// zhao_proj_cfgvalid.sv -- THE SHARED PROJECTOR'S CONFIG-VALID ARM: the
// producer entry I14 says `proj_en_i` has never had.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS AT ALL
// ---------------------------------------------------------------------------
// `zhao_console_core.sv`'s I14 entry carried a tie-off in these words:
//
//   > `proj_en_i` itself is still a tie-off and still has NO producer
//   > anywhere: every instantiation of `zhao_proj_subsystem` ... passes `en_i`
//   > straight through from its own port, and the smoke bench drives it with a
//   > literal 1.
//
// and `FINDINGS-projinput.md` decision D-1 put three readings in front of the
// owner and RECOMMENDED the second of them, in these words:
//
//   > (b) It becomes a CONFIG-VALID gate: hold the projector off until a
//   > view's matrix bank has been written, so nothing is projected through an
//   > unwritten bank (every vertex to screen centre with garbage depth). This
//   > is real function with a fireable counter, and the smoke bench already
//   > implements it as fake stimulus -- `geom_camera_ready_q` gates its draw
//   > "so no descriptor can be culled against an unwritten bank".
//
// This is that block. It is deliberately NOT written inside the composer, for
// the reason `zhao_measure_starve.sv` gives for itself: a composer may write a
// join whose two sides are the same cycle's wires, and may not write a
// register. The arm is state, so it belongs in a file with a contract and a
// test.
//
// READING (a) -- "the subsystem derives its own enable and the port is
// removed" -- WAS REJECTED BY D-1 AND IS REJECTED AGAIN HERE, for a reason
// the campaign's own rules give: removing the port removes function, and
// "a PORT is neither a module nor a tie-off" -- deleting one moves no number
// and costs the evidence that stood on it.
//
// ---------------------------------------------------------------------------
// THE ENABLE IS A FREEZE, NOT A DISCARD -- WHICH IS WHAT MAKES THIS SOUND
// ---------------------------------------------------------------------------
// `zhao_project_core.sv`'s own header states the semantic this block is about
// to drive:
//
//   > The pipeline is RIGID and its enable is the CALLER'S, taken as `en_i`:
//   > every stage advances together or none does, so a stalled consumer
//   > freezes the whole chain and NOTHING IS DROPPED OR REORDERED.
//
// and `zhao_project_service.sv` spends it as `take_a = en_i && grant_a &&
// core_ready`, so a low enable withholds the GRANT and back-pressures the
// client. Holding `en` low therefore delays vertices; it never loses them.
// That is the one property a config-valid gate needs and it is read off the
// producer's own text rather than assumed here.
//
// AND THE CONVERSE, WHICH IS THE DEADLOCK CHECK AND IS THE THING THAT WOULD
// HAVE BEEN COMFORTABLE TO SKIP. This block arms FROM the configuration bus
// and gates the vertex pipeline, so it deadlocks the instant configuration
// writes are themselves gated by the enable. THEY ARE NOT, and the projector
// says so in its own words at its refused-write counter:
//
//   > Refused writes, counted. Not gated by `en_i`: configuration writes are
//
// so the bank can always be written while the pipeline is frozen, and the arm
// can always be reached. The two quantities are on different gates ON PURPOSE.
//
// ---------------------------------------------------------------------------
// THE LAW: A COVERAGE MASK, NOT A COUNT AND NOT A CADENCE
// ---------------------------------------------------------------------------
// "A view's matrix bank has been written" is measured as: every one of the
// MAT_WORDS matrix addresses of SOME view has been written at least once since
// reset. It is a SET, tracked as a one-hot-set mask per view.
//
// A MASK RATHER THAN A COUNT, deliberately. `zhao_cmd_exec`'s view walk writes
// addresses 0..15 one per clock and the host port can overwrite any single
// word at any time with cycle priority (see the merge in the composer). A
// count of writes would read "16 words written" after a host poked address 3
// sixteen times, which is a bank with thirteen holes in it and an instrument
// saying it is full. The mask cannot be fooled that way: bit k is the claim
// "address k of this view has been written", and nothing but a write to
// address k sets it.
//
// A SET RATHER THAN A SEQUENCE, equally deliberately. This block never asks
// WHEN the words arrived or in what order, because both are the writer's
// business and both have already changed once: the walk grew from sixteen
// steps to seventeen (the depth profile at address 18) and then to twenty-one
// (the viewport rect at 16 and 17). A producer that had encoded "sixteen
// consecutive clocks" would have been silently wrong at each of those, with
// every counter still balancing. Only the addresses are the thing.
//
// ADDRESSES >= MAT_WORDS ARE IGNORED, AND THAT IS THE POINT. Addresses 16, 17
// and 18 are the viewport rectangle and the depth profile. They are real
// configuration and they are NOT the matrix, so they must not arm a bank whose
// camera is still zero -- projecting through a zero matrix is exactly the
// fault named above (every vertex to screen centre with garbage depth), and a
// viewport rectangle does nothing to prevent it. The bank this block speaks
// about is the one the sentence in I14 names: the MATRIX bank.
//
// EITHER VIEW ARMS IT, AND THIS IS THE ONE PLACE A READER SHOULD PUSH BACK, SO
// THE ARGUMENT IS WRITTEN OUT RATHER THAN ASSUMED. `en_o` is ONE wire for a
// RIGID pipeline that both views share, so it cannot be per-view without
// re-authoring `zhao_project_core`'s stage law -- stages in flight carry
// different views. That leaves two candidate laws and only one of them is
// safe:
//
//   * "every view's bank is written" -- conservative-looking, and it HANGS
//     THE CONSOLE. In a single-view presentation contract (Z60) `zhao_cmd_exec`
//     runs its view walk for view 0 only, so view 1's bank is never written,
//     so the projector would never arm and no frame would ever be drawn. The
//     comfortable-sounding choice is the broken one.
//   * "some view's bank is written" -- taken. Before it, NOTHING has been
//     configured and no projection can be correct for any view, which is the
//     fault this block exists to prevent. After it, WHICH view a vertex may
//     select is `zhao_cmd_exec`'s ordering obligation -- it writes a view's
//     bank under one dirty bit before issuing that view's draws -- and not the
//     projector's. A per-view interlock would be a second arming law for a
//     path that already has one.
//
// So the port's meaning is precisely "the projector has a camera", which is
// what D-1's sentence says and is a weaker claim than "the right camera". The
// weaker claim is the one this block can make from the configuration bus
// alone, and stating it narrowly is the alternative to over-claiming it.
//
// ---------------------------------------------------------------------------
// THE INSTRUMENTS, AND WHAT EACH ONE DISCRIMINATES (R95)
// ---------------------------------------------------------------------------
//   * `arm_events_o` -- 0 before the bank completes and 1 after, forever. It
//     is a POSITIVE CONTROL for the arm itself: a console whose projector runs
//     must read exactly 1, and a reading of 0 on a console that drew pixels
//     would mean the enable came from somewhere else.
//   * `held_offers_o` -- clocks on which a client OFFERED a vertex while the
//     bank was incomplete. This is the counter that proves the gate does
//     something rather than merely existing: it can only be non-zero if a real
//     vertex was actually withheld. It is reachable by LEGAL stimulus (offer
//     before configuring), so it needs no mutant.
//
// Neither is clocked by the other's operands: `arm_events_o` moves on the
// mask's completion and `held_offers_o` on a client's valid, which is the
// property CLAUDE.md's blind-detector chapter asks for.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_proj_cfgvalid #(
    // The number of MATRIX words in one view's bank. Sixteen is `mat4fx`, and
    // it is a knob rather than a literal because the bank's shape is the
    // projector's to change, not this block's to assume.
    parameter int unsigned MAT_WORDS = 16,
    // The projector's configuration address width. Five since the viewport
    // words landed; see I14.
    parameter int unsigned ADDRW     = 5
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the projector's configuration bus, TAPPED -------------------------
    // Nothing in this block drives the bus. It is the SAME merged bus the
    // subsystem is given -- host port and CMD.EXEC after the composer's
    // priority mux -- so the arm sees every write the bank sees, by
    // construction rather than by a second copy of the merge.
    input  wire                  cfg_we_i,
    input  wire                  cfg_view_i,
    input  wire [ADDRW-1:0]      cfg_addr_i,

    // ---- a client is OFFERING a vertex to the projector this cycle ---------
    // Purely instrumental: it is read by `held_offers_o` and by nothing else,
    // and no output depends on it. A composer that left it at zero would lose
    // the counter and change no behaviour.
    input  wire                  offer_i,

    // ---- the rigid-pipeline enable -----------------------------------------
    output wire                  en_o,

    // ---- instruments --------------------------------------------------------
    output wire [31:0]           arm_events_o,
    output wire [31:0]           held_offers_o
);

  // ---------------------------------------------------------------------------
  // ELABORATION GUARDS
  // ---------------------------------------------------------------------------
  // Inside `initial begin ... end` because Quartus 17.0 rejects a bare
  // module-scope elaboration check with "syntax error near text: `if`;
  // expecting `endmodule`" -- and `verilator --lint-only` returns 0
  // diagnostics on the bare form, so a clean lint says nothing whatever about
  // this. CLAUDE.md records both facts; they are obeyed here rather than
  // rediscovered.
  initial begin
    if (MAT_WORDS == 0)
      $fatal(1, "zhao_proj_cfgvalid: MAT_WORDS is 0; the bank would arm before any write");
    if (MAT_WORDS > (1 << ADDRW))
      $fatal(1, "zhao_proj_cfgvalid: MAT_WORDS %0d does not fit ADDRW %0d; bank addresses are unreachable and the arm can never complete",
             MAT_WORDS, ADDRW);
    // The mask index below is `cfg_addr_i[$clog2(MAT_WORDS)-1:0]`, which
    // addresses every bit of the mask ONLY when MAT_WORDS is a power of two.
    // At any other value the top bits of the mask would be unreachable and the
    // arm could never complete -- a hang, reported here rather than debugged
    // on a dark console.
    if ((1 << $clog2(MAT_WORDS)) != MAT_WORDS)
      $fatal(1, "zhao_proj_cfgvalid: MAT_WORDS %0d is not a power of two; mask bits above %0d would be unreachable and the bank could never arm",
             MAT_WORDS, (1 << $clog2(MAT_WORDS)) - 1);
  end

  // ---------------------------------------------------------------------------
  // THE PER-VIEW COVERAGE MASKS
  // ---------------------------------------------------------------------------
  logic [MAT_WORDS-1:0] seen_q [0:1];
  logic                 armed_q;
  logic [31:0]          arm_events_q;
  logic [31:0]          held_offers_q;

  // A matrix write is one to an address BELOW `MAT_WORDS`. The comparison is
  // made once, here, so the two users below cannot drift apart.
  wire matrix_write_c = cfg_we_i && ({{(32-ADDRW){1'b0}}, cfg_addr_i} < 32'(MAT_WORDS));

  // THE COMPLETION IS TESTED ON THE NEXT MASK, NOT THE HELD ONE, AND THE FIRST
  // VERSION OF THIS BLOCK GOT IT WRONG -- the directed lane caught it, which is
  // what the lane is for. Reading `seen_q` would arm the projector ONE CLOCK
  // AFTER the write that completed the bank, so there would be a dead cycle in
  // which the bank is demonstrably full and the enable is still low. Nothing
  // downstream would have failed -- a freeze is lossless -- which is exactly
  // why it would have stayed: an off-by-one with no symptom, in a block whose
  // whole job is to say WHEN the camera exists.
  logic [MAT_WORDS-1:0] seen_n [0:1];
  always_comb begin
    seen_n[0] = seen_q[0];
    seen_n[1] = seen_q[1];
    if (matrix_write_c)
      seen_n[cfg_view_i][cfg_addr_i[$clog2(MAT_WORDS)-1:0]] = 1'b1;
  end

  // The bank of EITHER view being complete arms the pipeline. See the header
  // for why this is a disjunction and not a conjunction; the conjunction hangs
  // a single-view contract.
  wire complete_c = (&seen_n[0]) || (&seen_n[1]);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      seen_q[0]     <= '0;
      seen_q[1]     <= '0;
      armed_q       <= 1'b0;
      arm_events_q  <= '0;
      held_offers_q <= '0;
    end else begin
      seen_q[0] <= seen_n[0];
      seen_q[1] <= seen_n[1];

      // THE ARM LATCHES AND NEVER FALLS. A bank that has been written once is
      // written for good: the next view walk overwrites words in place, and a
      // mid-walk de-assert would freeze the projector for the length of every
      // SetView -- a throttle nobody asked for, on a path whose ordering
      // obligation lives in CMD.EXEC's dirty bit and not here.
      if (complete_c && !armed_q) begin
        armed_q      <= 1'b1;
        arm_events_q <= arm_events_q + 32'd1;
      end

      // The withheld-offer counter. `armed_q` and not `complete_c`, so it
      // agrees with the enable the clients actually saw on this clock.
      if (offer_i && !armed_q)
        held_offers_q <= held_offers_q + 32'd1;
    end
  end

  assign en_o          = armed_q;
  assign arm_events_o  = arm_events_q;
  assign held_offers_o = held_offers_q;

endmodule : zhao_proj_cfgvalid

`default_nettype wire
