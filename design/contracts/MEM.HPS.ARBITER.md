# Contract — MEM.HPS.ARBITER (HPS bridge requester arbiter)

> Ledger: `design/blocks.yml` · owner ZH-077 · phase 2 · maturity UNIT_VERIFIED
>
> Design: `reports/DEBUG.FRAMEBLIT_Integration_Corrections.md` §7, Step 3.

## Purpose and exclusions

Give two requesters one HPS bridge port: choose an owner, convert the clients'
held ready/valid request into the single-cycle pulse the bridge requires, and
route every response beat to the owner and to nobody else.

**Why it exists.** Splitting the blit out of CMD.DMA left two blocks wanting the
bridge. `zhao_hps_bridge` has one client port and one burst in flight, and it
treats a request arriving while it is busy as a **protocol violation**, answered
with `err`. The loser of an unarbitrated race is not made to wait — it is told
its transfer failed.

**Not here:** the bridge itself, and the shell's decision about which block is
client 0. The mapping is encoded and tested here rather than assumed: even if
the scheduler's ordering makes the two mutually exclusive today, that is a
property of the scheduler and not of this wire.

## Clock and reset semantics

Single `clk`, asynchronous active-low `rst_n`, `gpu` domain. Reset returns to
`A_IDLE` with no owner; a burst in flight at reset is abandoned, and the bridge
is reset alongside.

## Input and output packet layouts

Two client ports, each `zhao_hps_burst_req_t` in / `req_grant` out /
`wr_valid`,`wr_data`,`wr_last` in / `zhao_hps_burst_rsp_t` out. One bridge port
of the same shape.

## Backpressure rules

**Clients hold; the bridge pulses.** This asymmetry is the block's main job.

- **Client side:** a client asserts its request and HOLDS it until
  `req_grant_o`. That is a normal ready/valid handshake and it is what
  DEBUG.FRAMEBLIT does.
- **Bridge side:** the request is asserted for **exactly one cycle**. The bridge
  accepts combinationally and sets `busy` on the same edge, then raises
  `req_grant` the *next* cycle — so a request still asserted when the grant
  arrives is a request-while-busy, which is a violation. CMD.DMA already knew
  this: its `hps_req_v` is set in one state and cleared by a default assignment
  the next cycle.

"Hold the request stable until grant" is therefore right for one side and
exactly wrong for the other, and getting them the wrong way round costs an `err`
on every single transfer.

## Memory ownership

**None.** A state register, an owner bit, a direction bit, three counters.

## Q formats and rounding

**None.**

## Latency (fixed or variable)

Two cycles of arbitration overhead — one to latch the owner, one to pulse the
request — then the bridge's own latency. Variable overall, dominated by the HPS.

## Target throughput

One burst at a time, which is all the bridge has.

## Overflow and malformed-input behaviour

**THE SIX RULES.**

1. **One owner at a time, and the owner is LATCHED.** Re-deciding while a burst
   is in flight is how a response beat reaches the wrong client.
2. **A response beat goes to the owner and nowhere else.** The one rule here
   that corrupts rather than stalls: a beat delivered to the wrong client is
   written into the wrong buffer with no error anywhere.
3. **The bridge request is a one-cycle pulse.** See backpressure above.
4. **An `err` without a grant still ends the transaction.** A malformed burst is
   rejected with `err | last`, **no grant** and **no busy**. An arbiter waiting
   only for the grant waits forever — and the symptom is not that transfer
   failing, it is the *next* one never happening.
5. **A write burst ends on `wr_last`, not on a response.** The bridge answers a
   write with nothing at all. An arbiter waiting for `rsp.last` hangs on the
   first write it ever arbitrates.
6. **Client 0 has strict priority, and the waiting is counted.**

## Counters and traces

`c0_bursts_o`, `c1_bursts_o`, and `c1_wait_cycles_o`.

**The wait counter is not decoration.** Strict priority guarantees client 0
never waits behind client 1 for a new burst; it guarantees **nothing** about
client 1 ever being served. A client 0 that asks continuously starves client 1
forever. In practice CMD.DMA asks about once per frame, so the starvation window
is bounded by *its* request pattern and not by anything here — which is a policy
whose safety depends on somebody else's behaviour, and therefore one that must
be **visible** rather than assumed. The directed test pins both halves: that the
priority is real, and that the starvation is real.

If that trade ever stops being acceptable, the fix is a bounded yield — after a
client-0 burst completes, let a waiting client 1 go next — which costs client 0
at most one blit burst of latency. It is not implemented because the review asks
for priority and a debug blit "may wait".

## Scalar reference function

**No scalar reference, and this is the right answer rather than a gap.** The
block has no arithmetic and no data transformation; it is a routing decision
over a protocol. Its correctness is a relationship between two RTL modules, so
the oracle it is checked against is the module `zhao_hps_bridge` itself,
composed in the test — recorded in the ledger as `rtl::zhao_hps_bridge`.

The ledger grew phantom citations by demanding a `zref::` name from blocks that
had none, so `rtl::` is a distinct claim rather than a waiver: V17 checks that
the named module is actually declared in an RTL source, exactly as it checks a
`zref::` symbol is defined under `reference/`.

## Directed tests

`tests/memory/hps_arbiter_directed.cpp` — 66 checks, composed with the **real**
`zhao_hps_bridge` via `tests/memory/zhao_hps_arb_compose.sv`.

Composing with the real bridge is the point. The property under test is a
protocol agreement *between* the arbiter and the bridge, and a fake bridge is
precisely the thing that agrees with whatever the arbiter does — the same
failure that let DEBUG.FRAMEBLIT ship with six defects. Neither of the two
bridge behaviours that shape this design (`err` on busy, `err` with no grant on
malformed) would exist in a permissive stub.

Each client's read data is a hash of its own address, so a beat routed to the
wrong client is a wrong **value**, not merely a wrong count. `hps_err_count_o`
is asserted zero in every well-formed section, which is what catches a request
held one cycle too long.

Sections: a single client; Case H contention in both orderings; adjacent
addresses so a split burst cannot look correct; a slow HPS at three delays; two
malformed shapes followed by proof the port still serves; strict priority and
its cost; a write burst and the port surviving it; a write contending with a
read.

Mutation sweep: **12 mutations, 12 caught, 0 survived, 0 discarded.**

**The sweep found a real bug.** `wr_from_wrong_client` survived the first run —
because no test issued a write at all. Chasing why led to the discovery that the
bridge answers a write with no response, so the arbiter's completion condition
would have hung the port on the first write it ever saw. Nothing in Phase 2
writes to HPS DDR, which is exactly why it was worth fixing rather than leaving:
an untested path with a latent hang is worse than an absent one, because it
looks finished.

## Randomized differential tests

None yet. The interesting axis is the *interleaving* of two clients against the
bridge's protocol, and the directed sections place each contention at a chosen
point. The gap worth filling later is randomised request timing on both clients
simultaneously with a randomly stalling HPS.

## Formal properties

None yet. The set worth proving: `b_req_o.valid` is never asserted two cycles
running; a response beat never reaches a non-owner; `owner` never changes
between grant and burst end; and every accepted request eventually reaches
`A_IDLE` (a liveness property, and the one that would have caught the write
hang directly).

## Synthesis / resource ceiling

Not yet fitted. A handful of flops and two muxes.

## Integration capture cases

None yet. The first is Step 4 of the integration order, with both real clients
attached.
