# Step 4 is built, works, and is faster — and that is the problem

> A decision for Fabian. Everything here is **simulation**; nothing has run on a
> board.
>
> The work is on branch **`blit-integration-step4`**, not on `main`. `main` is
> green.

## What was built

Step 4 of `reports/DEBUG.FRAMEBLIT_Integration_Corrections.md`:
`DEBUG.FRAMEBLIT` and `VIDEO.SLOTMGR` are wired into `zhao_shell_top`, the blit
dispatch moves off `CMD.DMA`, the guard window becomes the lease, slot readiness
comes from an accepted publication, the swap crosses `vid → gpu` as a toggle,
the write queue answers with a real `ready`, and retirement comes from the VRAM
arbiter's credit stream.

It works. Every blit completes with status 0 and every displayed frame is the
right picture.

## The measurement

| | first blit completes | period |
| --- | ---: | ---: |
| old path (`CMD.DMA`) | 605,308 | 637,184 |
| new path | **547,321** | 637,184 |

**The new blitter is ~58,000 gpu cycles faster.** I expected slower and the
measurement said otherwise.

The reason is the redesign itself. The old path fetched the whole canvas from
HPS (~93k cycles) and *then* committed it to VRAM (~245k), serially, because the
old atomicity rule forbade writing anything before the checksum passed. The new
path streams: fetch and commit overlap. `duo_markers.cpp`'s own header names
**"streaming blit CRC"** as one of the *ratification-scale paths that would close
60 Hz*. This is that path.

The frame **rate** does not change — `ticks=14, crcs=15` for six frames on both.
The **phase** moves by one frame.

## So what breaks

`demos/wound_lab/duo_markers.cpp` fails **41 of 340** checks at 40 frames. Not
one of them is a wrong pixel. Every displayed CRC at frame N now equals the
value expected at frame N+1 — the right picture, one frame early.

What fails is timing calibrated against a 338k-cycle blit:

- **the half-rate tick-repeat phase** — `tick repeated flag (half-rate law)`;
- **the deadline-faults closed form** — `deadline_faults = F+1`;
- **the `STATUS_DEADLINE` fence** on each frame.

And `reports/status/phase2_wave2.md` carries the dossier those numbers come
from: "a full-canvas Duo blit costs ~338k gpu cycles end to end … 60 Hz
fresh-frame cadence with zero deadline faults is therefore INFEASIBLE on the
composed Phase-2 machine as frozen."

**That cost is now ~58k lower.** Not enough to close 60 Hz — the commit phase
still dominates against a 318,592-cycle frame — but the dossier's headline
number is no longer the machine's number.

## Why I did not just regenerate the golden

Because the golden is not only a picture. It encodes a **measured property of
the machine** that the demo asserts as a law in code, and that a report explains
at length. Regenerating it would silently re-baseline that law, and the next
person to read the dossier would find numbers that no longer describe anything.

The picture being identical is exactly what makes this tempting and exactly why
it should not be done quietly.

## What I need from you

One of:

1. **Take it.** I regenerate the duo golden, update the demo's per-tick
   expectations to the new phase, and revise the half-rate dossier in
   `phase2_wave2.md` with the new measured cost. The machine gets faster and the
   record follows it.
2. **Keep the phase.** I hold the blit back so it lands in the same frame window
   as before. This throws away the improvement deliberately and says so.
3. **Something else** — if the demo's phase is load-bearing for a reason I have
   not seen, tell me and I will work to it.

I would take option 1. The improvement is real, it is the direction your own
review pushed, and the dossier being out of date is a fact whether or not I
merge this.

## What is on the branch beyond the wiring

**The one-cycle law this seam lives or dies by**, found by wiring it rather than
by reading anything: `DEBUG.FRAMEBLIT` latches the lease generation on the *same
edge* it accepts a request. So the lease must already be granted when the
request arrives. Asking for the lease and handing over the request on one edge
makes the blitter latch the generation from *before* the grant — and every
publication is then refused as stale by a slot manager working perfectly. There
is a four-state sequencer in the shell for exactly this. Two cycles per blit,
and the difference between a machine that works and one that quietly never shows
a frame.

## Already on `main`

The shell harness now stamps every blit completion with the gpu step it happened
on, and the demo prints blit periods. The redesign changes a **cost**, so the
harness has to be able to measure a cost rather than only report that a blit
finished. That is how the table above exists, and it touches no golden.
