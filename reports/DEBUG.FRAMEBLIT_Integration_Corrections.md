# DEBUG.FRAMEBLIT Integration Corrections


> **Agent: please read this file completely before integrating
> `DEBUG.FRAMEBLIT` into `zhao_shell_top`.**
>
> Reviewed against `main` at:
> `55cf8c9213ec9ff749713e6c4748a15f3e294edd`
>
> This is a correction and integration proposal, not a claim that the existing
> unit-tested block is useless. The central design is sound: stream into an
> invisible leased framebuffer slot, calculate the CRC in one pass, and publish
> only after the transaction succeeds. The implementation has, however, not yet
> closed several shell-level safety seams that its contract claims to close.


---


## Executive summary


`DEBUG.FRAMEBLIT` correctly removes the need for the old 1,966,080-bit
whole-canvas staging buffer. That is the right architectural direction.


Before it replaces the old blitter in `CMD.DMA`, the following issues need to be
resolved:


1. **Slot-1 writes currently use the wrong address.**
   `guard_req_o.addr` is slot-relative, while `MEM.GUARD` expects an absolute
   framebuffer address.


2. **The implementation does not wait for physical SDRAM retirement.**
   Its `retired` counter currently means “the chunk was handed downstream,” not
   “the SDRAM controller retired the writes.”


3. **Failure release and successful publication carry no lease identity.**
   A bare pulse is not enough to prove which slot and generation are being
   released or published.


4. **Abort currently releases immediately, even when writes may still be in
   flight.**
   That permits a slot to be reused while older writes can still arrive.


5. **A lease can lapse after the last explicit check and before the publication
   pulse.**
   Publication must check the live lease at the publication edge.


6. **Lease loss does not immediately stop external side effects.**
   The current block can finish reading and proceed toward guarded writes even
   after `lease_held` has been cleared.


7. **The HPS bridge request is not acknowledged.**
   The block asserts a request for one state and advances without observing the
   bridge’s real `req_grant`.


8. **The shell still instantiates the legacy blitter inside `CMD.DMA`.**
   The new block is unit-verified but is not yet part of the running machine, and
   the old full-canvas buffer still remains in the composed design.


The integration wave should correct these points, remove all blit machinery from
`CMD.DMA`, and immediately rerun composed Quartus synthesis.


---


# 1. Slot-1 addressing is currently wrong


The current RTL emits:


```systemverilog
guard_req_o.addr = zhao_pkg::ZHAO_VRAM_ADDR_BITS'(off);
```

That is a byte offset relative to the beginning of the selected framebuffer.

zhao_mem_guard, however, expects an absolute VRAM address. Its blit window
is:

slot 0: ZHAO_FB_SLOT0_BASE + [0, blit_span)
slot 1: ZHAO_FB_SLOT1_BASE + [0, blit_span)

Slot 0 happens to work because ZHAO_FB_SLOT0_BASE is zero. Slot 1 does not.
A request for slot 1 with off == 0 currently asks the guard to write address
zero, which belongs to slot 0. The real guard must reject it.

The unit test did not catch this because its fake guard returns an OK verdict
without validating the requested address.

Required correction

Use the leased slot’s absolute base:

logic [31:0] fb_base;


assign fb_base = r_slot[0]
               ? zhao_pkg::ZHAO_FB_SLOT1_BASE
               : zhao_pkg::ZHAO_FB_SLOT0_BASE;


assign guard_req_o.addr =
    zhao_pkg::ZHAO_VRAM_ADDR_BITS'(fb_base + off);

The address should be derived from the latched and validated lease target, not
from a live request input.

Required tests

Add address assertions for both legal slots:

slot 0, chunk 0 -> ZHAO_FB_SLOT0_BASE
slot 0, chunk N -> ZHAO_FB_SLOT0_BASE + 64*N


slot 1, chunk 0 -> ZHAO_FB_SLOT1_BASE
slot 1, chunk N -> ZHAO_FB_SLOT1_BASE + 64*N

Also instantiate the real zhao_mem_guard in at least one composition test. A
fake always-OK guard cannot prove the address law.

Mutation requirement:

Mutation: remove slot base and emit only `off`.
Expected: slot-1 composition test fails on the first guard request.
2. “Retired” currently does not mean retired

The amended atomicity law is:

No framebuffer slot becomes visible or READY before every byte has been
written, all writes have retired, and the CRC matches.

The current implementation does not enforce the “retired” part.

It currently advances retired in B_NEXT_CHUNK:

retired <= retired + 32'(this_len);

This happens after the write-data beats have been accepted by
guard_wready_i. It does not mean the SDRAM controller has completed the
writes.

B_WAIT_RETIRE then does not wait for either counter:

B_WAIT_RETIRE: begin
  if (!lease_held) begin
    ...
  end else begin
    state <= B_CRC_DECIDE;
  end
end

Therefore a slot can currently be published while its data is still sitting in:

the shell write-data FIFO;
the VRAM arbiter;
the SDRAM controller’s pending burst;
or the physical SDRAM write pipeline.
The required retirement source already exists

zhao_vram_arbiter returns credits when a controller burst retires:

client_rsp[k].credits

Those credits are measured in 16-bit SDRAM words.

The blit client is the arbiter’s ZHAO_CLIENT_BLIT_DMA port. The shell can route
that client’s returned credits to DEBUG.FRAMEBLIT.

MEM.GUARD currently ignores returned credits because it is only a request-path
guard. The retirement sideband may therefore either:

bypass the guard in zhao_shell_top; or
be exposed transparently through a new guard output.

The first is the smaller change.

Proposed interface

Add:

input logic [7:0] retire_words_i;

Every cycle:

if (retire_words_i != 8'd0) begin
  retired_bytes <= retired_bytes
                 + ({24'd0, retire_words_i} << 1);
end

One retired word is two bytes.

The retirement counter must update in every active state, not only
B_WAIT_RETIRE, because credits may return while the next source chunk is being
read.

Publication condition

The publication condition must be structurally equivalent to:

issued_bytes  == r_len
retired_bytes == r_len
lease_valid_at_this_exact_edge
lease_slot    == r_slot
lease_gen     == r_gen
computed_crc  == r_crc
no recorded failure

B_WAIT_RETIRE must remain in place until the first two equalities are true.

Required temporal test

Build a test in which:

every HPS read succeeds;
every guard request succeeds;
every write-data beat is accepted immediately;
all retirement credits are withheld after the final beat;
the test waits for hundreds of cycles;
blit_publish_o must remain low;
credits are then returned in several partial groups;
publication must remain low until the final credit;
publication may occur only after the last byte retires.

This test must fail against the current implementation.

Mutation requirement:

Mutation: publish when issued_bytes == len, ignoring retired_bytes.
Expected: delayed-retirement test fails.
3. Failure paths must drain accepted writes before releasing a slot

A transaction can fail after earlier chunks have already been accepted by the
memory system:

lease loss;
HPS bridge error on a later chunk;
guard denial on a later chunk;
reset or protocol error;
any future error introduced after some writes have been issued.

The current B_ABORT state immediately pulses:

fb_lease_release_o <= 1'b1;

and returns to B_IDLE.

That is unsafe if issued_bytes > retired_bytes.

A released slot can be leased to a new transaction while older writes from the
failed transaction are still in the FIFO, arbiter or SDRAM controller. Those old
writes can then land after the new owner starts writing, corrupting the new
framebuffer generation.

Required failure state split

Use at least:

B_ABORT_STOP
B_ABORT_DRAIN
B_RELEASE
B_ABORT_STOP
Record the first failure.
Stop issuing new HPS requests.
Stop issuing new guard requests.
Stop presenting new write-data beats.
If an HPS burst is already in flight, drain its response without consuming the
bytes into a future write.
Do not publish.
Do not release yet.
B_ABORT_DRAIN

Wait until:

retired_bytes == issued_bytes

No slot release is permitted before this equality.

B_RELEASE

Release the exact lease generation owned by this transaction, then complete the
request with its failure status.

A failure that occurs before any write was issued can pass through the drain
state immediately because both counters are zero.

Required test
Complete several chunks successfully.
Withhold some retirement credits.
Inject a bridge error or lease failure.
Confirm that:
no further guard requests appear;
no further write-data beats appear;
release stays low while credits are outstanding;
release occurs only after every accepted write retires;
publish never occurs.

Mutation requirement:

Mutation: release immediately on failure.
Expected: abort-drain test fails.
4. Release and publication need slot and generation identity

The current block outputs:

fb_lease_release_o
blit_publish_o

Both are bare pulses.

A pulse alone does not identify:

which framebuffer slot it applies to;
which lease generation it applies to;
whether the manager’s currently active lease is the one that originated the
transaction.

This becomes especially dangerous after an ABA sequence or any delayed pulse.

Recommended interface

Replace or accompany the bare pulses with:

output logic        release_valid_o;
output logic        release_slot_o;
output logic [15:0] release_generation_o;


output logic        publish_valid_o;
output logic        publish_slot_o;
output logic [15:0] publish_generation_o;

The slot manager must accept a release or publication only when:

slot_state[slot] == WRITING
slot_generation[slot] == supplied_generation

A stale publication or stale release is refused and traced.

Bad-length and no-lease behaviour

The current B_ABORT pulses release even for:

bad length;
no lease at start;
slot mismatch.

Those cases did not successfully acquire ownership and therefore must not release
an unrelated active lease.

There are two lawful implementations:

Preferred

Carry slot and generation on release, and let the manager validate identity.

Minimum

Track:

logic transaction_owns_lease;

Set it only after length, lease-validity, slot and generation validation all
succeed. Pulse release only when it is true.

The reference model and directed tests currently treat several pre-acquisition
failures as released = true. That semantic should be corrected if “release”
means an actual ownership transition. An error completion is not the same thing
as releasing a lease.

Required tests
Bad length while some unrelated slot has an active lease:
the unrelated lease must remain unchanged.
No lease at request start:
no release transition.
Slot mismatch:
no release of the lease belonging to the other slot.
Stale release generation:
manager refuses it.
Stale publication generation:
manager refuses it.
5. The lease must be live at the publication edge

The current implementation maintains lease_held, but the final state sequence
leaves a race:

B_WAIT_RETIRE
    -> B_CRC_DECIDE
    -> B_PUBLISH

A lease can lapse after B_WAIT_RETIRE checks lease_held and before
B_PUBLISH raises its pulse.

Because nonblocking assignments observe the old value during the state’s case
logic, clearing lease_held at the top of the sequential block does not
automatically prevent the same edge’s state transition or publication.

Required correction

Check the live lease at the exact publication decision:

if (!lease_ok_now || !lease_held) begin
  fail  <= ST_LEASE_LOST;
  state <= B_ABORT_STOP;
end else if (issued_bytes != r_len || retired_bytes != r_len) begin
  state <= B_WAIT_RETIRE;
end else if ((crc_acc ^ 32'hFFFF_FFFF) != r_crc) begin
  fail  <= ST_CRC;
  state <= B_ABORT_STOP;
end else begin
  publish_valid_o      <= 1'b1;
  publish_slot_o       <= r_slot[0];
  publish_generation_o <= r_gen;
  ...
end

The safest shape is to perform the final check and publication pulse in the same
state and on the same edge. A separate B_PUBLISH state is acceptable only if it
rechecks the live lease and generation again.

Required test

Drop or change the lease:

after the final retirement credit;
during CRC decision;
one cycle before the existing publication state.

No version of this sequence may publish.

Mutation requirement:

Mutation: check lease in B_WAIT_RETIRE but not at publication.
Expected: publication-edge lease-loss test fails.
6. Lease loss must stop further external side effects

The current per-cycle monitor clears lease_held when the lease is invalid, but
several states do not act on that information immediately.

For example, lease loss during B_READ_CHUNK can still allow the block to:

finish filling the chunk;
request a guarded write;
transmit that chunk;
continue until the final wait state notices lease_held == 0.

That violates the reason the lease exists.

Required rule

Once live lease validity is lost:

no new HPS request may be issued;
no new guard request may be issued;
no new write-data beat may be presented;
an already issued HPS burst must be drained or cancelled according to the
bridge law;
already accepted SDRAM writes must retire before release;
publication is permanently forbidden for that transaction.

Every side-effecting state should either check lease_ok_now directly or be
reachable only while a latched abort_pending is false.

A simple global rule is:

abort_pending <= abort_pending || !lease_ok_now;

Then gate all new external requests with:

!abort_pending && lease_ok_now

The state machine still needs explicit draining for operations already accepted.

7. HPS bridge requests need an acknowledgement and an owner

zhao_hps_bridge has one physical request port and one request-acceptance pulse:

req
req_grant
rsp

The current DEBUG.FRAMEBLIT interface has no grant input.

It asserts:

hps_req_o.valid = (state == B_READ_REQUEST);

and transitions to B_READ_CHUNK without observing whether the bridge accepted
the request.

This works only under an implicit assumption that:

the bridge is idle;
no CMD.DMA burst owns it;
the request is legal;
the bridge accepts it on that exact cycle.

That assumption should not be a hidden protocol.

Required shell-level arbitration

After the blit is split from CMD.DMA, both blocks can request the same HPS
bridge:

CMD.DMA             \
                     -> HPS request arbiter -> zhao_hps_bridge
DEBUG.FRAMEBLIT     /

The arbiter must:

choose one valid requester;
hold the selected request stable until req_grant;
latch the response owner;
route every response beat to that owner;
keep ownership until rsp.last or rsp.err;
never route a beat to the non-owner.
Required client interface

Add a request grant/ready input to each client, for example:

input logic hps_req_grant_i;

DEBUG.FRAMEBLIT must remain in B_READ_REQUEST until that grant occurs.

The request must remain stable under backpressure:

hps_req.valid && !hps_req_grant
    -> addr, len, client and write remain stable
Arbitration policy

Command-packet DMA should have priority during packet acquisition. A debug blit
is not game-facing and may wait.

A stronger optimization may prove that scheduler ordering makes the two clients
mutually exclusive. Even if that is true, encode and test the rule rather than
depending on it implicitly. The bridge grant still needs to be observed.

Required composition test
Hold the HPS bridge busy with a CMD.DMA burst.
Present a frame-blit request.
Confirm that the blit request remains stable and does not advance.
Retire the DMA burst.
Grant the blit request.
Confirm every response beat goes only to the blit.
Repeat with the opposite ordering.
8. Integrate the real write-data backpressure

The new block correctly has:

guard_wready_i

The current shell’s legacy write FIFO does not provide ready. It accepts
blit_wvalid unconditionally and sets a sticky error after overflow.

That old seam must not be retained around the new block.

Required shell ready calculation

One 64-bit write-data beat expands into four 16-bit FIFO words.

A conservative ready law is:

guard_wready =
    (wfifo_free_words >= 4);

Then enqueue only on:

guard_wvalid && guard_wready

While ready is low, the block must keep guard_wdata and guard_wlast stable.

The sticky overflow signal may remain as an integrity tripwire, but normal flow
must make overflow structurally unreachable.

Required assertion
guard_wvalid && !guard_wready
    -> $stable(guard_wdata) && $stable(guard_wlast)

The current unit test covers this in isolation. The shell composition must cover
it against the real FIFO capacity signal.

9. The framebuffer slot manager must become explicit

The block contract correctly says that the slot manager is not part of
DEBUG.FRAMEBLIT. It still needs to exist before the block can be integrated.

Required per-slot states
FREE
WRITING
READY
DISPLAYED

The meaningful transitions are:

FREE      -> WRITING   lease granted
WRITING   -> READY     matching publication
WRITING   -> FREE      matching release after drain
READY     -> DISPLAYED frame-control swap consumes it
DISPLAYED -> FREE      another slot becomes displayed

A slot may be leased only when it is:

FREE;
not currently displayed;
not READY;
not already being written;
not committed to the next swap.
Generation rule

Every transition into WRITING increments that slot’s generation.

The generation is part of:

the lease grant;
the publication event;
the release event.

No stale generation may alter current state.

Domain ownership

Choose exactly one clock domain to own the slot state. Do not divide one state
machine between gpu_clk and vid_clk.

Recommended shape:

slot manager state lives in gpu_clk, beside the blitter and guard;
publication becomes a synchronized ready event into vid_clk;
frame-control’s consumed/swap event returns as a synchronized toggle into
gpu_clk;
only single-bit events cross domains;
slot identity is latched and held stable around the toggle.

This follows the existing completion-toggle style but gives the ownership law an
explicit home.

Formal properties
A slot is never WRITING and DISPLAYED simultaneously.
A slot is never READY and WRITING simultaneously.
At most one slot is DISPLAYED.
A lease grant implies previous state FREE.
A publication changes WRITING -> READY only on matching generation.
A release changes WRITING -> FREE only on matching generation.
A stale generation changes no state.
A displayed slot is never leased.
10. Remove the old blitter from CMD.DMA completely

The new standalone block does not remove the original problem until the legacy
path is deleted from the composed cone.

At the reviewed head, zhao_cmd_dma.sv still contains:

BLIT_BUF_BYTES;
blit_buf;
blit_req_*;
blit_done_*;
guard_req_o;
guard_wdata_o;
guard_wvalid_o;
M_BLIT_CHK;
M_BLIT_REQ;
M_BLIT_WAIT;
M_BLIT_COMMIT;
M_BLIT_DATA;
the blit CRC gate;
the legacy blit counters and status.

zhao_shell_top.sv still wires the scheduler’s debug blit directly into
zhao_cmd_dma.

Required CMD.DMA result

After this integration wave, CMD.DMA should be packet-only:

FRAME_RING request
-> HPS packet fetch
-> packet-level validation
-> verified packet stream

It should have no framebuffer slot, guard or blit ports.

Deletion gate

The wave is not complete while any of these remain in CMD.DMA:

BLIT_BUF_BYTES
blit_buf
M_BLIT_*
blit_req_valid_i
blit_done_o
guard_wdata_o

The exact names may change, but the architectural rule is that the command front
end contains no full-frame transport path.

11. Shell integration order

Perform the integration in this order so each step has a clear failure surface.

Step 1 — Correct the standalone block

Before shell wiring:

fix absolute slot addressing;
add retirement input;
add publication/release slot and generation;
add request-grant input for HPS;
add abort/drain states;
close the publication-edge lease race;
stop side effects immediately on lease loss;
correct the reference model’s release semantics;
extend directed tests;
add the key formal properties.
Step 2 — Build the slot manager

Implement and test:

FREE -> WRITING -> READY -> DISPLAYED -> FREE

Include generation matching and stale-event refusal.

Step 3 — Build the HPS requester arbiter

Compose:

CMD.DMA
DEBUG.FRAMEBLIT
zhao_hps_bridge

Prove response ownership and stable requests.

Step 4 — Wire the real memory path

Connect:

DEBUG.FRAMEBLIT.guard_req
    -> MEM.GUARD
    -> MEM.VRAM.ARBITER
    -> MEM.SDRAM


DEBUG.FRAMEBLIT.guard_wdata
    -> ready-aware shell write FIFO
    -> MEM.SDRAM write-data input


MEM.VRAM.ARBITER blit credits
    -> DEBUG.FRAMEBLIT.retire_words
Step 5 — Wire publication into frame control

A successful matching publication moves the slot to READY.

A failed transaction:

publishes nothing;
waits for all accepted writes to retire;
releases only its exact lease generation.
Step 6 — Remove legacy CMD.DMA blit logic

Only after the new path passes shell tests.

Step 7 — Rerun the complete shell verification

Run:

standalone directed tests;
randomized stall tests;
formal properties;
shell golden tests;
long Duo run;
bad-CRC invisible-slot composition;
good-CRC delayed-retirement composition;
command/HPS contention test.
Step 8 — Rerun composed Quartus synthesis immediately

Do not start another large greenfield feature before obtaining the new composed
resource result. Removing the old full-canvas buffer is the exact intervention
needed to move past the previous failure.

12. Required integration cases
Case A — Slot-1 address
slot 0 displayed
slot 1 leased
good source
real MEM.GUARD

Every guard request must be inside:

[ZHAO_FB_SLOT1_BASE,
 ZHAO_FB_SLOT1_BASE + canvas_bytes(mode))
Case B — Bad CRC remains invisible
slot 0 displayed
slot 1 leased
bad expected CRC

Prove:

slot 0 displayed CRC never changes;
slot 1 is never READY;
publication never pulses;
slot 1 releases only after all issued writes retire;
dirty bytes in slot 1 are accepted and remain invisible.
Case C — Publication waits for retirement
good CRC
all write-data accepted
retirement credits delayed

Prove:

no publication while credits remain outstanding;
final credit is necessary;
publication occurs once;
the published generation matches the lease.
Case D — Lease loss during source read

Drop the lease after a source beat but before the chunk ends.

Prove:

the current HPS burst is drained safely;
the chunk is not submitted to the guard;
no further HPS request occurs;
previous accepted writes drain;
no publication;
release occurs only after drain.
Case E — Lease loss after final retirement

Drop the lease between:

last retirement
CRC decision
publication

No publication is allowed.

Case F — ABA regrant
slot 1, generation 41
lease disappears
slot 1 reappears, generation 42

The generation-41 transaction must never publish or release generation 42.

Case G — Pre-acquisition errors

Test:

bad length;
no lease;
wrong slot.

None may release or publish an unrelated lease.

Case H — HPS bridge contention

Hold CMD.DMA’s burst in flight while the blitter requests the bridge.

Prove:

no dropped request;
no duplicate request;
no response delivered to the wrong client;
blitter starts only after a real grant.
Case I — Backpressure

Randomize simultaneously:

HPS response stalls;
guard request stalls;
write FIFO stalls;
SDRAM retirement timing.

The final framebuffer and CRC must remain bit-identical.

13. Formal property set

The standalone formal harness should at minimum prove:

publish_valid
    -> live lease valid at the same edge


publish_valid
    -> publish_slot == latched lease slot


publish_valid
    -> publish_generation == latched lease generation


publish_valid
    -> issued_bytes == requested_length


publish_valid
    -> retired_bytes == requested_length


publish_valid
    -> computed_crc == expected_crc


publish_valid
    -> no failure recorded


release_valid
    -> transaction previously acquired that exact lease


release_valid
    -> retired_bytes == issued_bytes


release_valid and publish_valid are never both true


guard_req.valid
    -> live matching lease


guard_req.valid
    -> absolute address lies inside the leased slot


guard_wvalid && !guard_wready
    -> data and last remain stable


lease loss
    -> no future new HPS request


lease loss
    -> no future new guard request


lease loss
    -> no future new write-data beat


CRC failure
    -> never publish


bridge failure
    -> never publish


guard denial
    -> never publish


reset during transaction
    -> never publish

The shell composition should additionally prove or assert:

displayed slot is never leased for writing


slot generation changes only on a new lease


stale publish/release events change no slot state


each accepted blit request produces exactly one terminal event


terminal event is either publish or failure, never both


no slot becomes READY before physical write retirement
14. Synthesis acceptance gate

The redesign is motivated by a synthesis failure, so passing unit tests alone is
not the final acceptance.

Focused fit

Fit zhao_debug_frameblit and record:

ALMs;
registers;
M10Ks;
DSPs;
fMAX;
fit time;
whether the chunk buffer inferred as RAM, LUTRAM or registers.

The 512-bit buffer may reasonably remain registers. The important result is that
there is no full-canvas structure.

Composed shell fit

Acceptance requires:

zhao_shell_top instantiates zhao_debug_frameblit.
zhao_cmd_dma contains no full-canvas blit buffer.
zhao_scanout_linebuf still infers block RAM.
Quartus no longer reports the old CMD.DMA blit_buf uninferred-memory error.
Analysis and synthesis complete.
The fitter produces actual composed:
ALM usage;
register usage;
M10K usage;
DSP usage;
timing results.
The complete transcript and machine metadata are committed under
reports/composed/.

A failed fit is still useful evidence and should be committed. The crucial
difference is that it must fail for the next real resource or timing reason, not
because a debug path contains two million fake registers.

15. Non-goals for this wave

Do not broaden this integration into unrelated work.

Not required here:

a second ping-pong chunk buffer;
optimizing the CRC datapath;
widening CMD.DMA’s packet stream;
building the complete CMD.DECODER integration;
optimizing the 171-DSP estimate;
particle or compositor work;
board-specific PLL or pin assignments.

Those may follow. This wave should close one thing completely:

Replace the old framebuffer blitter with a safe, physically retired,
lease-identified transactional blitter and obtain the first meaningful
cleaned-up composed synthesis result.

Final verdict

The new architecture is the correct one, but the current implementation has not
yet reached the contract it describes.

The central corrections are:

absolute framebuffer address
real SDRAM retirement credits
drain before release
identity-bearing release/publication
lease check at the publication edge
stop side effects immediately after lease loss
acknowledged HPS request ownership
explicit slot manager
real shell integration
legacy CMD.DMA blitter deletion
composed synthesis

Do these before treating DEBUG.FRAMEBLIT as the completed replacement for the
old path.

The most important falsification test is simple:

Accept every write-data beat, withhold every retirement credit, and prove the
slot cannot become READY.

The current implementation should fail that test. The corrected implementation
must pass it.
