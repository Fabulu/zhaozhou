# DEBUG.FRAMEBLIT — response to the integration corrections

> Answering `reports/DEBUG.FRAMEBLIT_Integration_Corrections.md`.
>
> Everything below is **simulation**. Nothing has run on a board.

## Verdict first

**The review is correct.** I checked each structural claim against the code
rather than taking it on trust, and the ones I could verify directly all hold:

| # | Claim | Verified how |
| --- | --- | --- |
| 1 | Slot-1 writes use the wrong address | `zhao_mem_guard.sv:112` requires `addr32 >= blit_base`; `blit_base` is `ZHAO_FB_SLOT1_BASE = 0x0200_0000`. The block emitted the bare offset. **Every slot-1 request would have been denied.** |
| 2 | "Retired" did not mean retired | The counter advanced in `B_NEXT_CHUNK` from the block's own `this_len`. `zhao_vram_arbiter` does return per-client credits (`ctrl_rsp.credits`), so the real signal existed and was simply not used. |
| 7 | The bridge request was never acknowledged | `zhao_hps_bridge.sv:37` has `req_grant`, "registered accept pulse". The block had no grant input. |

Points 3–6 are about the block's own logic and are correct on inspection.

The diagnosis behind them is the part worth keeping: **every model in my test
harness was more agreeable than the real thing.** The fake guard returned OK
without looking at the address. The fake bridge granted instantly. The fake
memory retired instantly — in fact the block counted its own hand-offs as
retirements, so "retirement" was never modelled at all. A model that says yes to
everything cannot fail a test, and 43 checks passed against a block that was
wrong in six ways.

## Step 1 — correct the standalone block: **done**

| Review item | Status |
| --- | --- |
| 1. Absolute slot addressing | Done. `guard_req_o.addr = fb_base + off`, base from the latched lease target. |
| 2. Retirement input | Done. `retire_words_i` (u8, 16-bit words), accumulated in **every** state. |
| 3. Abort/drain states | Done. `B_ABORT_STOP` → `B_ABORT_DRAIN` → `B_RELEASE`; nothing touches the lease until `retired == issued`. |
| 4. Publish/release slot + generation | Done. `publish_valid/slot/generation_o`, `release_valid/slot/generation_o`. |
| 5. Pre-acquisition failures release nothing | Done. `owns_lease` gates release; the reference model was corrected too. |
| 6. Lease live at the publication edge | Done. The final check and the pulse are one state, one edge. |
| 6b. Lease loss stops side effects | Done. `abort_pending` gates every new request, **and** `guard_req_o.valid` is gated on the live lease so none is even asserted. |
| 7. HPS request grant | Done. The request is held until `hps_req_grant_i`. |
| 8. Reference model release semantics | Done. `BlitOutcome::acquired`; the three pre-acquisition failures no longer report `released`. |
| 9. Extended directed tests | Done — see below. |
| 10. Formal properties | Done. 27 assertions, depth 44, 8 covers all reached. |

### The harness now refuses things

- **The guard checks the address** on every request, against the leased slot's
  base, for **both** slots — slot 0's base is zero, which is the whole reason a
  slot-relative address looked correct for so long.
- **The bridge makes the block wait** (`grant_delay`), and the block must hold
  its request stable.
- **The memory returns credits** that can be delayed, held, or frozen forever.
- **The guard can be slow to accept** (`guard_ready_delay`), which is the window
  in which a lapsed lease must pull a request back down.

### Tests

97 checks, up from 43. New sections cover the review's cases A–H apart from the
ones that need blocks that do not exist yet:

- slot-0 **and** slot-1 base addresses, and every chunk inside the slot window;
- publication waits for retirement, including credits withheld for hundreds of
  cycles after the final beat;
- credits withheld **forever** — the blit must never complete, never publish;
- failure with writes outstanding: no release until the drain finishes, and
  release names its slot and generation;
- credits frozen forever after a failure: the slot is **never** released;
- lease lost at the publication edge specifically;
- a slow guard with the lease lost while it is deciding;
- grant delayed by 1, 3 and 17 cycles;
- bad length / no lease / slot mismatch each release **nothing**.

### Mutation sweep

Thirteen mutations, one per defect the review named, including its four explicit
"mutation requirement" lines. **11 caught, 2 recorded equivalent, 0 discarded.**

The two equivalents, recorded so they do not read as holes later:

- `guard_request_after_loss` — removing the state-level abort check in
  `B_GUARD_REQUEST`. It survives because `guard_req_o.valid` is *also* gated
  combinationally, and removing **that** gate is caught. The state check saves
  an idle cycle, nothing more.
- `publish_generation_live` — publishing the live generation instead of the
  latched one. It cannot differ, because publication already requires
  `lease_ok_now`, which is false unless the two are equal.

One correction to my own harness surfaced while doing this: it inspected
`guard_req_o` *before* `eval()`, so once the request depended on live lease
inputs it was comparing this cycle's lease against last cycle's request. That
reports the violation one cycle late and misses the one that mattered.

### Formal

`tests/formal/debug_frameblit_safety.sby` proves the set in your §13 — **27
assertions to depth 44, and 8 covers all reached**, which is the part that
matters: every assertion is an implication, so a model that cannot publish would
satisfy them all while proving nothing.

A publication is reachable only because of a FORMAL-ONLY canvas shrink. At the
real 184,320-byte canvas a transaction is 2,880 chunks and 46,000+ cycles, so no
bounded model reaches one — the same trap that made MEM.GUARD's lane and
CMD.DMA's assertion (b) vacuous. The scope guard `a_scope_single_chunk` fires if
anyone widens the canvas without re-justifying the depth.

Two findings from writing it:

- `a_pub_nofail` failed at k = 2 until the model was constrained to start from a
  real reset — unconstrained registers let it publish out of a fabricated state.
- The state-level abort check in `B_GUARD_REQUEST` is provably redundant given
  the combinational gate, which settles one of the two surviving mutations as
  genuinely equivalent rather than a hole.

## Step 2 — the slot manager: **done**

`VIDEO.SLOTMGR` (`fpga/rtl/video/zhao_video_slotmgr.sv`), reference
`zref::video::SlotManager`, contract `design/contracts/VIDEO.SLOTMGR.md`. It is
the ninetieth block and the authority DEBUG.FRAMEBLIT's whole safety argument
rested on: something has to decide that the slot being written speculatively is
not the one on screen, and nothing did.

`FREE → WRITING → READY → DISPLAYED → FREE`, per-slot generation, one clock
domain (`gpu`, as you recommended — the synchronizers stay the shell's so this
block is a pure single-clock machine that can be proven as one).

**68 directed checks + 28,290 random against the reference. Mutation sweep 14/14
caught, 0 survived, 0 discarded. Formal: depth 24, 8 covers all reached.**

Six of your eight formal properties are proven. The other two — "never WRITING
and DISPLAYED at once", "never READY and WRITING at once" — are true by the
two-bit state encoding, so asserting them would prove a property of
`logic [1:0]` rather than of this design. They are **named in the RTL** and
become real assertions the moment anyone moves to one-hot.

### The formal lane found a real defect

A publication and a release in the **same cycle** raced: two state writes in one
cycle, the later one silently winning, so a slot could go FREE on the very edge
it was told to become READY.

DEBUG.FRAMEBLIT proves it never emits both (its `a_excl`) — but this block is
the authority on slot ownership and must not rest on a peer behaving. The pair
is now refused and counted once, because it is one bad event and not two.

Two further findings were in my properties rather than the design, and both are
worth knowing:

- `$past(state[$past(slot)])` is ambiguous about whether the index is sampled
  now or then. It read as a live assertion and behaved as something else. Every
  such property now uses explicit previous-cycle shadow registers.
- Those shadows needed the DUT's own reset. Without it they recorded events that
  arrived *while reset was asserted* — which the DUT correctly ignored — and the
  counterexample was a grant pulse that never happened.

### One directed gap the mutation sweep found

A first draft exercised `slot_ready` on slot 1 only, so a mutation breaking slot
0's bit alone walked through the entire directed set and was caught by the
random lane. That is the wrong place for something that simple to be caught.
Both slots are covered now.

## What remains

**Steps 1 and 2 are complete.**

**Steps 3–8 are not started**, and they are the larger part:

| Step | What it needs |
| --- | --- |
| 2. Slot manager | **Done** — `VIDEO.SLOTMGR`, block 90. See below. |
| 3. HPS requester arbiter | CMD.DMA and DEBUG.FRAMEBLIT share one bridge port; response ownership must be proven. |
| 4. Real memory path | guard → arbiter → SDRAM, and a ready-aware shell write FIFO. |
| 5. Publication into frame control | |
| 6. Remove the legacy blit from CMD.DMA | `BLIT_BUF_BYTES`, `blit_buf`, `M_BLIT_*` and the guard ports still exist there. |
| 7. Full shell verification | |
| 8. Composed Quartus re-fit | The acceptance evidence for the whole redesign. |

**On the review's Step 8 instruction** — "do not start another large greenfield
feature before obtaining the new composed resource result" — noted, and it
changes what I do next. The composed fit cannot run until Step 6 deletes the old
blitter, so the ordering stands as written.

One thing to flag: the review's own note that the shell still instantiates the
legacy blitter means **none of this is in the running machine yet**. The
standalone block is correct and verified in isolation; that is all it is.
