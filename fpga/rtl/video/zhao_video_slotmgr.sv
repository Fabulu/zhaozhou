// zhao_video_slotmgr.sv — VIDEO.SLOTMGR: who owns a framebuffer slot, and when.
//
// Contract:  design/contracts/VIDEO.SLOTMGR.md
// Design:    reports/DEBUG.FRAMEBLIT_Integration_Corrections.md §9 (Step 2)
// Reference: `zref::video::SlotManager` (reference/include/zref/zref_slotmgr.hpp)
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK HAD TO EXIST BEFORE ANYTHING ELSE COULD BE WIRED
// ---------------------------------------------------------------------------
// DEBUG.FRAMEBLIT writes speculatively into a slot and publishes only if
// everything went right. That is safe ONLY because the slot it writes to is not
// being looked at — and until now nothing in the machine actually decided that.
// The shell granted the guard a window at dispatch and cleared it at done, with
// no generation, no notion of DISPLAYED and no way to refuse a stale event. The
// blitter's whole safety argument rested on a seam that did not exist.
//
// This block is that authority. It answers one question, and every other
// framebuffer rule reduces to it:
//
//     WHO OWNS THIS SLOT RIGHT NOW, AND IS THAT STILL THE SAME OWNER?
//
// ---------------------------------------------------------------------------
// THE STATES, and the only lawful transitions
// ---------------------------------------------------------------------------
//     FREE      -> WRITING     a lease is granted        (generation++)
//     WRITING   -> READY       a MATCHING publication
//     WRITING   -> FREE        a MATCHING release
//     READY     -> DISPLAYED   frame control swaps to it
//     DISPLAYED -> FREE        another slot becomes displayed
//
// ---------------------------------------------------------------------------
// FIVE THINGS ARE LOAD-BEARING
// ---------------------------------------------------------------------------
// 1. **THE GENERATION INCREMENTS ON EVERY ENTRY INTO WRITING** — not per lease
//    request, not per frame. It is what makes an event attributable: a
//    publication carrying generation 41 is refused once the slot has moved on
//    to 42, even though the slot number and the state are identical. Without it
//    a blit that lost its lease and one that never lost it look the same at the
//    moment they publish, which is the ABA hole DEBUG.FRAMEBLIT's lease check
//    exists to close — and closing it there is worthless if this end does not
//    close it too.
//
// 2. **A STALE EVENT CHANGES NOTHING, AND IS COUNTED.** Refusing it silently
//    turns a lease bug into a frame that mysteriously never appears. The
//    counter is the difference between "the machine is protecting itself" and
//    "something is wrong and nobody can tell which".
//
// 3. **ONLY A `FREE` SLOT IS LEASABLE.** That single condition subsumes "not
//    displayed, not READY, not already being written, not committed to the next
//    swap". Those are not four checks; they are four names for `state != FREE`.
//    Writing them separately is how one of them ends up missing.
//
// 4. **THE DISPLAYED SLOT IS FREED AT THE SWAP, NOT AT THE PUBLICATION.** A
//    buffer stops being visible when something else is shown, not when a
//    replacement becomes available. Freeing it early hands a still-visible
//    buffer to the next blit, which is exactly the corruption the whole lease
//    scheme exists to prevent.
//
// 5. **ONE LEASE AT A TIME.** Phase 2 has one blitter. Two slots in WRITING
//    with one writer is a bookkeeping error, not a capability, so the grant is
//    refused while a lease is outstanding.
//
// ---------------------------------------------------------------------------
// CLOCK DOMAIN
// ---------------------------------------------------------------------------
// ONE domain owns this state — `gpu`, beside the blitter and the guard. Frame
// control lives in `vid`, so its swap arrives here as an already-synchronized
// event and the readiness leaving here is synchronized on the way out. Nothing
// in this machine is split across domains: a two-domain ownership FSM is a race
// with extra steps. The synchronizers themselves are the shell's, not this
// block's, so that this block stays a pure single-clock state machine that can
// be proven as one.
module zhao_video_slotmgr (
    input logic clk,
    input logic rst_n,

    // ---- lease request (from the blit dispatch path) -----------------------
    input  logic        lease_req_valid_i,
    input  logic        lease_req_slot_i,
    output logic        lease_grant_o,        // one pulse
    output logic        lease_refused_o,      // one pulse: the slot was not FREE

    // ---- the live lease, presented to DEBUG.FRAMEBLIT ----------------------
    output logic        fb_lease_valid_o,
    output logic        fb_lease_slot_o,
    output logic [15:0] fb_lease_generation_o,

    // ---- terminal events from DEBUG.FRAMEBLIT ------------------------------
    input  logic        publish_valid_i,
    input  logic        publish_slot_i,
    input  logic [15:0] publish_generation_i,
    input  logic        release_valid_i,
    input  logic        release_slot_i,
    input  logic [15:0] release_generation_i,

    // ---- the swap, already synchronized from the video domain ---------------
    input  logic        swap_valid_i,
    input  logic        swap_slot_i,

    // ---- outputs -----------------------------------------------------------
    output logic [1:0]  slot_ready_o,      // level, crossed to vid by the shell
    output logic        displayed_valid_o,
    output logic        displayed_slot_o,
    output logic [1:0]  slot_state_o [0:1],  // for tracing

    // ---- counters ----------------------------------------------------------
    output logic [31:0] leases_granted_o,
    output logic [31:0] stale_events_o     // events refused, see law 2
);

  localparam logic [1:0] S_FREE = 2'd0;
  localparam logic [1:0] S_WRITING = 2'd1;
  localparam logic [1:0] S_READY = 2'd2;
  localparam logic [1:0] S_DISPLAYED = 2'd3;

  logic [ 1:0] state       [0:1];
  logic [15:0] generation  [0:1];
  logic        displayed_v;
  logic        displayed_s;
  logic        lease_active;
  logic        lease_slot;
  logic [15:0] lease_gen;

  assign slot_ready_o[0]      = (state[0] == S_READY);
  assign slot_ready_o[1]      = (state[1] == S_READY);
  assign slot_state_o[0]      = state[0];
  assign slot_state_o[1]      = state[1];
  assign displayed_valid_o    = displayed_v;
  assign displayed_slot_o     = displayed_s;
  assign fb_lease_valid_o     = lease_active;
  assign fb_lease_slot_o      = lease_slot;
  assign fb_lease_generation_o = lease_gen;

  // An event is honoured only if the slot is WRITING **and** the generation
  // matches. Law 1: the state alone cannot tell an ABA re-grant from an
  // uninterrupted lease.
  function automatic logic event_ok(input logic slot, input logic [15:0] gen);
    event_ok = (state[slot] == S_WRITING) && (generation[slot] == gen);
  endfunction

  // A publication and a release in the SAME cycle is not a lawful pair: one
  // transaction has one outcome. DEBUG.FRAMEBLIT proves it never emits both --
  // its `a_excl` -- but this block is the authority on slot ownership and must
  // not depend on a peer behaving. Both asserted together is REFUSED and
  // counted, exactly like any other malformed event.
  //
  // Without this the two writes race in one cycle and the later one silently
  // wins, so a slot could go FREE on an edge where it was told to become READY.
  // The formal lane found it: a_publish_ready failed at step 4.
  logic both_events;
  assign both_events = publish_valid_i && release_valid_i;

  logic pub_ok, rel_ok, swap_ok, grant_ok;
  assign pub_ok   = publish_valid_i && !both_events &&
                    event_ok(publish_slot_i, publish_generation_i);
  assign rel_ok   = release_valid_i && !both_events &&
                    event_ok(release_slot_i, release_generation_i);
  assign swap_ok  = swap_valid_i && (state[swap_slot_i] == S_READY);
  // Law 3 and law 5 together, and they really are the whole admission rule.
  assign grant_ok = lease_req_valid_i && !lease_active && (state[lease_req_slot_i] == S_FREE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state[0] <= S_FREE;
      state[1] <= S_FREE;
      generation[0] <= 16'd0;
      generation[1] <= 16'd0;
      displayed_v <= 1'b0;
      displayed_s <= 1'b0;
      lease_active <= 1'b0;
      lease_slot <= 1'b0;
      lease_gen <= 16'd0;
      lease_grant_o <= 1'b0;
      lease_refused_o <= 1'b0;
      leases_granted_o <= 32'd0;
      stale_events_o <= 32'd0;
    end else begin
      lease_grant_o <= 1'b0;
      lease_refused_o <= 1'b0;

      // ---- the swap ------------------------------------------------------
      // Taken first so that a slot freed by this swap is leasable on the very
      // next cycle rather than one frame later.
      if (swap_ok) begin
        // Law 4: the OUTGOING slot is freed here, at the swap, and nowhere
        // else. A buffer stops being visible when something else is shown.
        if (displayed_v && (displayed_s != swap_slot_i)) begin
          state[displayed_s] <= S_FREE;
        end
        state[swap_slot_i] <= S_DISPLAYED;
        displayed_v <= 1'b1;
        displayed_s <= swap_slot_i;
      end else if (swap_valid_i) begin
        // Frame control asked for a slot that is not READY. Nothing moves.
        if (stale_events_o != 32'hFFFF_FFFF) stale_events_o <= stale_events_o + 32'd1;
      end

      // ---- publication ---------------------------------------------------
      if (pub_ok) begin
        state[publish_slot_i] <= S_READY;
        lease_active <= 1'b0;
      end else if (publish_valid_i && !both_events) begin
        // Law 2: refused, and COUNTED. A silently dropped publication is a
        // frame that never appears with nobody able to say why.
        if (stale_events_o != 32'hFFFF_FFFF) stale_events_o <= stale_events_o + 32'd1;
      end

      // ---- release -------------------------------------------------------
      if (rel_ok) begin
        state[release_slot_i] <= S_FREE;
        lease_active <= 1'b0;
      end else if (release_valid_i && !both_events) begin
        if (stale_events_o != 32'hFFFF_FFFF) stale_events_o <= stale_events_o + 32'd1;
      end

      // The unlawful pair counts ONCE, not once per signal: it is one bad
      // event, not two.
      if (both_events) begin
        if (stale_events_o != 32'hFFFF_FFFF) stale_events_o <= stale_events_o + 32'd1;
      end

      // ---- the lease grant -----------------------------------------------
      if (grant_ok) begin
        state[lease_req_slot_i] <= S_WRITING;
        generation[lease_req_slot_i] <= generation[lease_req_slot_i] + 16'd1;
        lease_active <= 1'b1;
        lease_slot <= lease_req_slot_i;
        lease_gen <= generation[lease_req_slot_i] + 16'd1;
        lease_grant_o <= 1'b1;
        if (leases_granted_o != 32'hFFFF_FFFF) leases_granted_o <= leases_granted_o + 32'd1;
      end else if (lease_req_valid_i) begin
        // Refusing is normal, not an error: there may be no free slot this
        // frame. It gets its own pulse rather than sharing the stale counter,
        // because "no slot was available" and "somebody sent a stale event"
        // are different problems with different fixes.
        lease_refused_o <= 1'b1;
      end
    end
  end

`ifdef FORMAL
  // ---------------------------------------------------------------------------
  // THE INVARIANTS (reports/DEBUG.FRAMEBLIT_Integration_Corrections.md §9)
  // ---------------------------------------------------------------------------
  // Two of the eight properties in that list -- "a slot is never WRITING and
  // DISPLAYED simultaneously" and "never READY and WRITING simultaneously" --
  // are true by the ENCODING here: a slot holds one two-bit state, so it cannot
  // be in two at once. Asserting them would prove a property of `logic [1:0]`
  // rather than of this design. They are named here rather than written, and
  // they would become real assertions the moment anyone moved to a one-hot
  // encoding.
  //
  // The other six are properties of the machine, and they are below.
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // Previous-cycle shadows: what the event WAS, and what the slot it named
  // looked like at that moment. Written out rather than expressed with nested
  // `$past`, which is ambiguous about whether the index is sampled now or then.
  logic        f_pub_v, f_pub_slot, f_quiet;
  logic [ 1:0] f_pub_state;
  logic [15:0] f_pub_gen, f_pub_slot_gen;
  logic        f_rel_v, f_rel_slot, f_quiet_r;
  logic [ 1:0] f_rel_state;
  logic [15:0] f_rel_gen, f_rel_slot_gen;
  logic [31:0] f_stale;
  logic        f_pair;
  logic        f_grant, f_grant_slot;
  logic [ 1:0] f_grant_prev_state;
  logic [15:0] f_grant_prev_gen;

  // Reset the shadows the same way the DUT resets. Without this they record
  // events that arrived WHILE reset was asserted -- the DUT correctly ignored
  // them, the shadows did not, and the first counterexample was a grant pulse
  // that never happened. The shadow of a machine has to be reset with it.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_pub_v <= 1'b0;
      f_rel_v <= 1'b0;
      f_grant <= 1'b0;
      f_pair  <= 1'b0;
      f_quiet <= 1'b0;
      f_quiet_r <= 1'b0;
      f_stale <= 32'd0;
      f_pub_slot <= 1'b0; f_pub_gen <= 16'd0; f_pub_state <= 2'd0; f_pub_slot_gen <= 16'd0;
      f_rel_slot <= 1'b0; f_rel_gen <= 16'd0; f_rel_state <= 2'd0; f_rel_slot_gen <= 16'd0;
      f_grant_slot <= 1'b0; f_grant_prev_state <= 2'd0; f_grant_prev_gen <= 16'd0;
    end else begin
    f_pub_v        <= publish_valid_i;
    f_pub_slot     <= publish_slot_i;
    f_pub_gen      <= publish_generation_i;
    f_pub_state    <= state[publish_slot_i];
    f_pub_slot_gen <= generation[publish_slot_i];
    f_quiet        <= !swap_valid_i && !release_valid_i && !lease_req_valid_i;
    f_pair         <= publish_valid_i && release_valid_i;

    f_rel_v        <= release_valid_i;
    f_rel_slot     <= release_slot_i;
    f_rel_gen      <= release_generation_i;
    f_rel_state    <= state[release_slot_i];
    f_rel_slot_gen <= generation[release_slot_i];
    f_quiet_r      <= !swap_valid_i && !publish_valid_i && !lease_req_valid_i;

    f_stale        <= stale_events_o;

    f_grant            <= grant_ok;
    f_grant_slot       <= lease_req_slot_i;
    f_grant_prev_state <= state[lease_req_slot_i];
    f_grant_prev_gen   <= generation[lease_req_slot_i];
    end
  end

  always_ff @(posedge clk) begin
    if (!f_past_valid) assume (!rst_n);
    if (f_past_valid && $past(rst_n)) assume (rst_n);
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      // At most one slot is on screen. Two would mean the scanout has a choice
      // to make that nothing in the machine is authorised to make.
      a_one_displayed: assert (!((state[0] == S_DISPLAYED) && (state[1] == S_DISPLAYED)));

      // The displayed bookkeeping agrees with the states.
      if (displayed_v) begin
        a_displayed_agrees: assert (state[displayed_s] == S_DISPLAYED);
      end else begin
        a_none_displayed: assert ((state[0] != S_DISPLAYED) && (state[1] != S_DISPLAYED));
      end

      // A lease grant implies the slot was FREE the instant before -- which is
      // the single condition that subsumes "not displayed, not READY, not
      // already being written".
      // Shadows again, for the same reason: `$past(generation[lease_slot])`
      // samples the INDEX in the past too, and `lease_slot` in the past is the
      // previous lease's slot, not this one's.
      if (f_grant) begin
        a_grant_pulse:         assert (lease_grant_o);
        a_grant_was_free:      assert (f_grant_prev_state == S_FREE);
        // Law 1: the generation moved, by exactly one.
        a_grant_gen_moved:     assert (generation[f_grant_slot] == (f_grant_prev_gen + 16'd1));
        a_grant_gen_is_lease:  assert (lease_gen == generation[f_grant_slot]);
        a_grant_slot_writing:  assert (state[f_grant_slot] == S_WRITING);
      end

      // A live lease always points at a slot that is WRITING.
      if (lease_active) begin
        a_lease_writing: assert (state[lease_slot] == S_WRITING);
      end

      // A publication moves WRITING -> READY, and ONLY on a MATCHING
      // generation; a release moves WRITING -> FREE the same way; and a stale
      // event moves nothing at all.
      //
      // These are stated with explicit previous-cycle registers rather than
      // nested `$past(state[$past(slot)])`. The nested form is ambiguous enough
      // that it is not worth arguing about -- it read as a live assertion and
      // behaved as something else -- and the registers below say exactly what
      // was true, for which slot, on which edge.
      if (f_pub_v && !f_pair && (f_pub_state == S_WRITING) && (f_pub_slot_gen == f_pub_gen)) begin
        a_publish_ready: assert (state[f_pub_slot] == S_READY);
      end

      if (f_rel_v && !f_pair && (f_rel_state == S_WRITING) && (f_rel_slot_gen == f_rel_gen)) begin
        a_release_free: assert (state[f_rel_slot] == S_FREE);
      end

      // The ABA property: same slot, same state, wrong generation, and NOTHING
      // moves. `f_quiet` excludes the other three event kinds, so the only
      // thing that could have moved this slot is the stale publication itself.
      if (f_pub_v && !f_pair && (f_pub_state == S_WRITING) && (f_pub_slot_gen != f_pub_gen) && f_quiet) begin
        a_stale_publish_inert:   assert (state[f_pub_slot] == S_WRITING);
        a_stale_publish_counted: assert (stale_events_o != f_stale);
      end

      if (f_rel_v && !f_pair && (f_rel_state == S_WRITING) && (f_rel_slot_gen != f_rel_gen) && f_quiet_r) begin
        a_stale_release_inert:   assert (state[f_rel_slot] == S_WRITING);
        a_stale_release_counted: assert (stale_events_o != f_stale);
      end

      // The generation NEVER moves except on a grant. A number that drifts on
      // its own cannot be used to attribute anything.
      if (!lease_grant_o) begin
        a_gen0_stable: assert (generation[0] == $past(generation[0]));
        a_gen1_stable: assert (generation[1] == $past(generation[1]));
      end

      // Readiness means READY, exactly.
      a_ready0: assert (slot_ready_o[0] == (state[0] == S_READY));
      a_ready1: assert (slot_ready_o[1] == (state[1] == S_READY));
    end
  end

  // ---- non-vacuity covers --------------------------------------------------
  // Every assertion above is an implication. A model that can never grant,
  // publish, swap or go stale satisfies all of them while proving nothing.
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_grant:     cover (lease_grant_o);
      c_refuse:    cover (lease_refused_o);
      c_writing:   cover (state[0] == S_WRITING);
      c_ready:     cover (state[0] == S_READY);
      c_displayed: cover (state[0] == S_DISPLAYED);
      c_both_used: cover ((state[0] == S_DISPLAYED) && (state[1] == S_READY));
      c_stale:     cover (stale_events_o != 32'd0);
      c_handover:  cover (displayed_v && (displayed_s == 1'b1) && (state[0] == S_FREE));
    end
  end
`endif

endmodule : zhao_video_slotmgr
