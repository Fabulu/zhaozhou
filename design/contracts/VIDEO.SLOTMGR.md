# Contract — VIDEO.SLOTMGR (Framebuffer slot manager)

> Ledger: `design/blocks.yml` · owner ZH-076 · phase 2 · maturity UNIT_VERIFIED
>
> Design: `reports/DEBUG.FRAMEBLIT_Integration_Corrections.md` §9, Step 2. This
> block did not exist before that review named it.

## Purpose and exclusions

Own the framebuffer slot ownership state machine —
`FREE → WRITING → READY → DISPLAYED → FREE` — with a per-slot generation; grant
leases; accept or refuse publication and release events by slot **and**
generation; present slot readiness to VIDEO.FRAMECTL.

**Why it had to exist before anything else could be wired.** DEBUG.FRAMEBLIT
writes speculatively into a slot and publishes only if everything went right.
That is safe *only* because the slot it writes to is not being looked at — and
nothing in the machine actually decided that. The shell granted the guard a
window at dispatch and cleared it at done, with no generation, no notion of
DISPLAYED, and no way to refuse a stale event. The blitter's whole safety
argument rested on a seam that was not there.

This block answers exactly one question, and every other framebuffer rule
reduces to it: **who owns this slot right now, and is that still the same
owner?**

**Not here:** the swap *decision*. VIDEO.FRAMECTL decides which READY slot is
shown and when; this block records the consequence. Nor the CDC synchronizers —
they are the shell's, so that this block stays a pure single-clock machine that
can be proven as one.

## Clock and reset semantics

Single `clk`, asynchronous active-low `rst_n`, **`gpu` domain** — beside the
blitter and the guard, per the review's recommendation. Frame control lives in
`vid`, so its swap arrives here already synchronized and the readiness leaving
here is synchronized on the way out. Nothing in this machine is split across
domains: a two-domain ownership FSM is a race with extra steps.

Reset: both slots FREE, both generations zero, nothing displayed, no lease.

## Input and output packet layouts

**Lease request**: `lease_req_valid_i`, `lease_req_slot_i`. Answers with
`lease_grant_o` or `lease_refused_o`, each a one-cycle pulse.

**The live lease**, for DEBUG.FRAMEBLIT: `fb_lease_valid_o`, `fb_lease_slot_o`,
`fb_lease_generation_o` (u16).

**Terminal events**, from DEBUG.FRAMEBLIT: `publish_valid_i` / `publish_slot_i` /
`publish_generation_i`, and the same three for release.

**The swap**, already synchronized from `vid`: `swap_valid_i`, `swap_slot_i`.

**Outputs**: `slot_ready_o` (2-bit level, crossed to `vid` by the shell),
`displayed_valid_o`, `displayed_slot_o`, `slot_state_o[0:1]` for tracing.

## Backpressure rules

**None, by design.** Every input is an event, not a stream: a lease request is
answered the same cycle, and a publication, release or swap is either honoured
or refused on arrival. There is nothing to stall, because there is no queue —
this block holds four bits of state and two counters.

## Memory ownership

**None.** Two 2-bit states, two 16-bit generations, one displayed pointer, one
lease record, two counters. Under a hundred flops.

## Q formats and rounding

**None.** No fixed-point value passes through this block.

## Latency (fixed or variable)

**Fixed, one cycle.** Every event is resolved on the edge it arrives.

## Target throughput

One event per clock, and no rate is promised beyond that because none is needed:
the machine sees at most a handful of events per frame.

## Overflow and malformed-input behaviour

**THE FIVE LAWS.**

1. **The generation increments on every entry into WRITING** — not per lease
   request, not per frame. It is what makes an event attributable: a publication
   carrying generation 41 is refused once the slot has moved on to 42, even
   though the slot number and the state are identical. Without it a blit that
   lost its lease and one that never lost it look the same at the moment they
   publish. That is the ABA hole DEBUG.FRAMEBLIT's lease check exists to close,
   and closing it there is worthless if this end does not close it too.

2. **A stale event changes nothing, and is counted.** Refusing it silently turns
   a lease bug into a frame that mysteriously never appears. The counter is the
   difference between "the machine is protecting itself" and "something is wrong
   and nobody can tell which".

3. **Only a `FREE` slot is leasable.** That single condition subsumes "not
   displayed, not READY, not already being written, not committed to the next
   swap". Those are not four checks; they are four names for `state != FREE`.
   Writing them separately is how one of them ends up missing.

4. **The displayed slot is freed at the swap, not at the publication.** A buffer
   stops being visible when something else is shown, not when a replacement
   becomes available. Freeing it early hands a still-visible buffer to the next
   blit — exactly the corruption the lease scheme exists to prevent.

5. **One lease at a time.** Phase 2 has one blitter. Two slots in WRITING with
   one writer is a bookkeeping error, not a capability.

**A publication and a release in the same cycle is refused, and counted once.**
One transaction has one outcome. DEBUG.FRAMEBLIT proves it never emits both (its
`a_excl`), but this block is the authority on slot ownership and must not rest
on a peer behaving. The formal lane found this: with both asserted the two state
writes raced in one cycle and the later one silently won, so a slot could go
FREE on the very edge it was told to become READY.

**Event ordering within a cycle is part of the law**: the swap is applied first,
against the state as it was at the start of the cycle, then publication, then
release, then the grant. So a swap can never consume a slot that only became
READY on that same edge, and a grant never sees a slot freed on the edge it
asks — it becomes leasable the next cycle.

## Counters and traces

`leases_granted_o` and `stale_events_o` (u32, saturating). `slot_state_o` is
exposed for tracing because "the frame did not appear" is otherwise
unactionable.

`lease_refused_o` is a separate pulse rather than part of the stale count: "no
slot was available" and "somebody sent a stale event" are different problems
with different fixes, and pooling them hides both.

## Scalar reference function

`zref::video::SlotManager` — `reference/include/zref/zref_slotmgr.hpp`.

A new reference for a new block. It models one event at a time; the concurrent
publish+release case has no scalar expression and is pinned by the directed test
and the formal lane instead.

## Directed tests

`tests/video/video_slotmgr_directed.cpp` — 68 checks, plus 28,290 with
`--random 4000`.

Sections: reset state; a lease attempted from every one of the four states; the
generation moving on six consecutive leases; **the ABA case** (a re-granted slot
refusing the dead lease's publication and release); every other stale shape;
the displayed slot staying displayed while a replacement is READY; `slot_ready`
on both slots; a swap and a publication on the same edge; a publication and a
release on the same edge; and a random event stream against the reference,
comparing both states, readiness, lease liveness and both counters every step.

Mutation sweep: **14 mutations, 14 caught, 0 survived, 0 discarded.**

One directed gap the sweep found: a first draft exercised `slot_ready` on slot 1
only, so a mutation breaking slot 0's bit alone walked through the whole
directed set and was caught by the random lane — the wrong place for something
that simple. Both slots are covered now.

## Randomized differential tests

The `--random` lane above, one event per cycle against the reference. It asserts
its own coverage — a run in which nothing was granted, published, released,
swapped or refused would pass every comparison while exercising nothing.

## Formal properties

`tests/formal/video_slotmgr_invariants.sby`, lane `formal_video_slotmgr_invariants`,
recorded in `design/formal_runs.yml`. Depth 24, **8 covers all reached**.

Proven: at most one slot DISPLAYED and the bookkeeping agreeing with the states;
a grant implies the slot was FREE; a grant moves that slot's generation by
exactly one and nothing else moves it; a live lease always points at a WRITING
slot; publication and release move state **only** on a matching generation; a
stale event changes nothing and is counted; `slot_ready` means READY exactly.

Two of the review's eight — "never WRITING and DISPLAYED at once", "never READY
and WRITING at once" — are true by the two-bit encoding and are **named in the
RTL rather than asserted**, because asserting them would prove a property of
`logic [1:0]` and not of this design. They become real assertions the moment
anyone moves to one-hot.

Three findings while writing the lane, all in the properties rather than the
design at first, and one in the design:

- `$past(state[$past(slot)])` is ambiguous about whether the index is sampled
  now or then. Every such property is written with explicit previous-cycle
  shadow registers instead.
- Those shadows needed the same reset as the DUT. Without it they recorded
  events that arrived *during* reset, which the DUT correctly ignored, and the
  counterexample was a grant pulse that never happened.
- **The real one:** concurrent publish+release raced. See above.

## Synthesis / resource ceiling

Not yet fitted. Under a hundred flops and no memory by construction; it will not
be what a fit fails on.

## Integration capture cases

None yet. The first is Step 5 of the integration order: a successful matching
publication moving a slot to READY and FRAMECTL swapping to it, with the
outgoing slot freed and immediately leasable.

## Packet-G V2 successor (excluded, not yet adopted)

`zhao_video_slotmgr_v2` and `zhao_fb_ready_cdc_v2` are the Packet-G successor
pair. They are unit-verifiable infrastructure for the sibling shell in Packet H;
the historical manager and protected shell above remain unchanged and selected.
Nothing in this section claims production adoption.

### V2 lease authority and immutable identity

The manager remains wholly in the GPU clock domain. It admits held requests from
both writers: writer 0 is the blitter and writer 1 is the renderer. Simultaneous
requests alternate by the last **accepted contention** result; exactly one source
sees READY. A losing request held through the winning response is accepted later
as a typed refusal while that lease is live, and that solo disposition does not
erase contention priority. One held response carries:

```text
{writer1, granted1, slot1, generation16, mode2, base32, span32}
```

A granted lease becomes live only when that response is accepted. A stalled
response is immutable, and no younger request can overwrite it. There is still
one live lease globally. A legal request can be refused because another lease is
live, the requested slot is not FREE, or mode 3 is illegal; refusal is an accepted
response and creates no ownership state.

The caller supplies no framebuffer address. The manager derives the complete
window from the accepted slot and mode:

| mode | span |
|---:|---:|
| 0 (Z60) | 184,320 bytes |
| 1 (Storm) | 153,600 bytes |
| 2 (Duo) | 196,608 bytes |

Slot 0 base is `0x00000000`; slot 1 base is `0x02000000`. The accepted response,
live lease, READY event, stored slot record, displayed record, and swap echo all
carry the same immutable `{writer,slot,generation,mode,base,span}` identity. The
generation increments modulo 16 bits on each accepted grant into WRITING,
including wrap from `0xffff` to zero.

### Fault, terminal, and publication law

Fault and terminal keys are exactly `{writer,slot,generation}`. A nonmatching
fault, terminal, or swap is consumed, changes no ownership state, and increments
the modulo-32-bit stale-event count. A matching fault sticks in the live lease.
The terminal stream has one explicit `publish` bit and one immediate `fault` bit:

* matching cancel (`publish=0`) releases WRITING to FREE;
* matching clean publication moves WRITING to READY;
* a latched fault, terminal fault, or matching fault on the **same edge** as a
  publication wins and releases to FREE;
* every fault/cancel release emits zero READY events.

A clean terminal accepted while the READY hold is empty offers its 84-bit tuple
to the CDC on that same edge. If the CDC accepts, no duplicate is retained. If it
stalls, the tuple is held. If an older held tuple pops while a new publication is
accepted, the new tuple replaces it without a bubble or loss. Thus one clean
publication creates exactly one READY FIFO write, never a raw writer-done pulse.

Swap input is accepted and checked against a READY slot's complete
`{writer,slot,generation,mode,base,span}` record. Only an exact echo becomes
DISPLAYED and frees a different previously displayed slot. Omitting writer,
generation, mode, base, or span is a stale event, not a partial match.

Lifecycle counters are reset-zero modulo-32-bit observations:
requests/responses accepted, leases granted/refused, matching faults, clean
publications, releases, READY events, accepted swaps, stale events, and request
contentions. After response drain,
`requests_accepted == responses_accepted == leases_granted + leases_refused`.

### V2 bidirectional CDC and reset barrier

The CDC carries one exact 84-bit tuple in each direction:

```text
{writer1, slot1, generation16, mode2, base32, span32}
```

READY travels GPU-to-video; the accepted swap echo travels video-to-GPU. Each
channel uses a depth-four Gray-pointer asynchronous FIFO built from one
`zhao_dc_sdp_ram` (`DATA_W=84`, `ADDR_W=2`) plus its registered-read pending and
held-output state. One tuple may sit in the output hold, so RAM admission limits
live RAM ownership to three and the total channel ownership to four. Full and
empty decisions use only locally clocked pointers and three-stage synchronized
remote Gray pointers. Payload bits never cross except through the dual-clock RAM.

Both FIFO outputs are held under destination backpressure, preserve order, and
emit each accepted tuple once. The source must hold valid and payload while
READY is low. A valid drop or payload change after a genuinely stalled source
sets that source domain's sticky protocol fault; each detector has a directed
positive control. Enqueue/dequeue counters are independent per channel. A domain
is idle only when its locally owned outgoing memory is empty and its synchronized
incoming empty, pending read, and held output all prove no work.

Either external reset asserts a shared pair reset immediately. Pair reset then
feeds independent three-flop reset-release synchronizers; every data, pointer,
barrier, counter, output, and protocol-fault flop deasserts reset only on its own
clock. Each domain raises a local three-cycle up level and synchronizes the peer's
level through three flops. Source READY and RAM read issue remain closed until
both local and peer acknowledgements form that domain's barrier-done level.
Reset during FIFO occupancy, pending RAM read, held output, or held source offer
discards all pre-reset events. A source held through reset can be accepted only
after the new paired barrier, and no lease may be granted by Packet H before the
required acknowledgements.

### V2 verification boundary

`tests/video/video_slotmgr_v2_directed.cpp` covers both writers and arbitration
histories, response stalls, every slot/mode/window, grant/refusal, exact live
identity, stale writer/slot/generation, sticky and same-edge fault priority,
clean same-edge READY, publication backpressure, all swap fields, displayed
identity, generation wrap, reset gating, and drained counter equations.

`tests/video/fb_ready_cdc_v2_directed.cpp` uses independent clocks and phases. It
covers exact depth-four fill, both-direction backpressure and ordering,
pop/reload, reset with occupancy/pending-read/held-output/held-input, post-reset
barriers, changed clock ratios, idle, counter equality, and both stalled-source
fault controls.

`tests/video/packet_g_connected_directed.cpp` composes the real V2 manager and
CDC with the real VIDEO.FRAMECTL and two real MEM.GUARD instances. It proves
empty boundaries repeat without swap, one clean READY crosses and produces one
unchanged full-tuple echo/display update, later boundaries do not duplicate it,
a same-edge fault release cannot reach video, both captured writer polarities
gate the common framebuffer window, exact lower/upper boundaries hold, and reset
discards video-pending and held-echo work. Its raw-terminal-valid seam mutant
recreates the historical publication bypass and must reach video while the
manager correctly refuses the stale return.

Committed selector shims independently break terminal writer/generation, fault
publication, derived base, swap generation, captured READY writer, FIFO full,
barrier gating, READY generation, and read-pointer advance. Each runtime control
passes only after observing its named defect. The full-guard inverse has separate
behavioral and exact RAM-ownership-assertion controls, proving the three-in-RAM
bound before the held fourth tuple. Dual-selector controls must fail
elaboration with one exact collision diagnostic. These V2 modules remain
`not-yet-adopted`; Packet H owns their first shell composition and any connected
resource/timing claim.
