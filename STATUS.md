# Status — for Fabian

*This file is the channel. I update it and push; you read it here. Newest section
at the top.*

---

## 2026-08-25 (early hours) — the Field engine got faster and grew its last Earth piece

### The number

| | yesterday | now |
| --- | ---: | ---: |
| Field sequencer clock | 33.98 MHz | **36.84 MHz** |
| its logic area | 4,821 ALM | **4,673 ALM** |
| multipliers / memories | 3 / 4 | unchanged |

For context on the day before that: this same block was **8.59 MHz** when it was
first measured properly. It is now **4.3x** that.

**This particular step was small on purpose and it delivered what was predicted
for it**, +8%. I wrote down beforehand that it would help and would not reach
100 MHz, because the slow arithmetic units were untouched. That is what happened.

### What the timing report then told me, which I would have got wrong by planning

The plan said the next job was the square-root unit. **The report says it is the
sine unit** — 80 of the critical path's parts are inside `sine`, and *zero* are
in the square root, the reciprocal, the curve, the noise, the ring, the
rotation, the arithmetic core, the multiplier, the normaliser or the length
unit. Not "mostly"; zero.

That is the second time this week the measurement has re-ordered the plan. Had I
worked the written order instead, I would have spent a day speeding up a unit
that contributes nothing to the current bottleneck. **The plan ranks; the
measurement decides.**

### The three Earth writes are now real hardware

`MATERIAL`, `NAV` and `HAZARD` — the three answers you gave me on the 24th about
how overlapping field programs combine — exist in silicon terms now, with 21,345
checks against the reference and eleven deliberate sabotage variants, all
eleven caught.

They cost **no multipliers**, which matters because multipliers are the scarce
thing on this chip.

### Two pieces of unflattering bookkeeping, because they change what you can trust

**1. A test that looked like hardware proof was not.** A file sitting among the
hardware tests, named like them, passing — but it only ever exercised the
*reference model*. There was no hardware in it, because the hardware did not
exist yet. Anything I had advanced on its strength would have been resting on
nothing. The real hardware test is now written and is what those numbers above
come from.

**2. I wrote a comment claiming a bug that was not there.** I asserted that an
alternative design would open a timing hole. A repository rule refused the claim
because I had not named anything that would catch it. Rather than name something
plausible, I built the alternative: **it passes all 1,127 checks.** My claim was
wrong. I also built a deliberately broken version, which fails immediately —
without that second run, "the alternative passed" would have been
indistinguishable from "nothing tests this area at all". Both are written down.

### Where the whole machine stands

- **Frame blitter: finished** — verified in the assembled system, with its
  atomicity law sabotage-tested 20 out of 20.
- **Field instruction set: finished** — every operation built, tested and
  sharing one arithmetic unit instead of ten.
- **Field sequencer: working, and being made faster.** Sine is next.
- **Sixteen blocks still to write from scratch.** There is no backlog of
  nearly-done work left; that ran out on the 24th.
- **Particles, compositor and the 2D blocks remain untouched**, because they
  need decisions from you and I am not going to invent game behaviour for them.

### The one thing still red

The vertex-cache proof fails at depth 4. Its automated lane is deliberately
switched **off** rather than marked passing, so nothing in the repository claims
that block is proven. It is not.

---

## 2026-08-24 (evening) — the shell is at 98 MHz and the timing verdict finally means something

### The clock-domain repair worked, and it is measured

| | before | after |
| --- | ---: | ---: |
| graphics clock | ~94.6 MHz | **~98.0 MHz** |
| worst timing margin | −0.570 ns | **−0.208 ns** |
| failing paths | 36 | **23** |
| **hold failures** | **1 — FAILING** | **0 — PASSES** |

**We are 2% short of 100 MHz**, from 17% short this morning.

### The part that matters more than the number

That counter crossing between the video and graphics clocks used to be a **coin
flip**. Across four builds that changed nothing about it, it failed, passed,
passed, failed — a swing of 1.2 nanoseconds decided purely by where the chip
happened to place two flip-flops.

So the tool's verdict was **random**. Worse than always-red: a red you can plan
around, but a verdict that flips on luck can't report a real regression — and it
had already hidden one, concealing the true worst path behind its own noise.

It is now a proper handshake: the video side puts a value in a box and locks it,
raises one flag, and the graphics side takes the value and acknowledges. Only
that single flag crosses between clocks. **The crossing has disappeared from the
timing report entirely** — not improved, gone. Zero cross-clock paths remain in
the hold analysis.

And the proof is machine-checked rather than argued: it cannot lose a snapshot,
duplicate one, invent one, or let the video side overwrite a value the graphics
side hasn't taken.

### The one honest caveat

I proved the *protocol*. I did not prove the electrical behaviour of two
unrelated clocks sampling each other — no tool can; that is what the three-stage
synchroniser is for. And I have not yet tested it with the two clocks at
deliberately awkward speed ratios. Both gaps are written down in the repo rather
than left implied.

### Also today

* **A multiplier measurement killed a bad plan of mine.** I had proposed capping
  islands at 2 km to save multipliers. Measurement says that would have bought
  **nothing** — shrinking one side of a multiply while the other stays wide costs
  exactly the same. Your ruling stands: no island cap, no precision loss.
* **One real multiplier saving so far: 12 → 6** in the triangle binner, from
  multiplying a 23-bit value at 23 bits instead of at its 36-bit register width.
  I predicted 4 and got 6; I had miscounted the loop.

---

## 2026-08-24 (evening) — the renderer has a timing number for the first time, and it is 31 MHz

### What was measured

Terrain tessellator + normals, built as **one machine** with the seam between
them internal. Every renderer number before today was a single block measured in
isolation, with its outputs treated as free pins.

| | two blocks measured separately | **built as one** |
| --- | ---: | ---: |
| logic (ALMs) | 2,100 | **1,523** |
| multipliers | 24 | **9** |
| fake boundary pins | **1,045** | **67** |
| speed | **never measured** | **31.1 MHz** |

**94% of that boundary was fictional.** That is why isolated block numbers were
never trustworthy, and it is the thing the audit warned about in June and nobody
had tested until now.

The multiplier count is a clean confirmation: 6 + 3 = 9, exactly as predicted
after this morning's normals rework took that block from 18 to 3.

### 31 MHz against a 100 MHz target — what it does and does not mean

**It is not a throughput problem.** Even at 31 MHz this pair can produce about
**74,000 normals per frame against a demand of 2,000** — 37× more than the game
asks for. The work fits easily.

**It is a problem because the clock is shared.** If this pair sits in the same
clock domain as everything else, it sets the ceiling for everything else, and
that ceiling would be 31 MHz rather than 100.

**Which is good news, oddly.** A block with 37× spare throughput can be given
more pipeline stages almost for free: each stage costs a clock of latency and
buys clock speed, and we have latency to burn. This is the cheapest kind of
timing problem to fix — unlike the shell, where the last 5% is spread thin
across a dozen unrelated paths.

### What it changes about priorities

The shell is at 95.9 MHz and 4% short. **The renderer is 3× short and nobody
knew, because no renderer block had a valid speed measurement at all.** That
inverts the ordering: the shell's remaining 4% is a polish problem, while the
renderer needs real pipelining work — and it needs the same pair-wise
measurement done for the other seams before we can say how much.

Five more pairs are worth building the same way: texture+cache, fragment+tile
store, setup+binner, projector+vertex cache, field+terrain intake.

---

## 2026-08-24 (late afternoon) — one line of RTL took the shell from 836 failing paths to 16

### Measured, before and after, on the same machine and the same flow

| | before | after |
| --- | ---: | ---: |
| worst setup slack | −1.991 ns | **−0.423 ns** |
| failing setup endpoints | **836** | **16** |
| worst hold slack | −0.952 ns (failing) | **+0.254 ns (passes)** |
| failing hold endpoints | 1 | **0** |
| graphics clock | 83.4 MHz reported / ~92 real | **95.9 MHz** |

**The hold failure is gone and we are 4% off the 100 MHz target**, from 17% off
this morning.

### What the change was

The glue that chops incoming command packets into records asked, every clock:

> is this byte position past 36, **and** past (packet length − 4)?

and the answer gated a 144-bit write. The subtraction sat directly in front of
the comparison, so the chip had to do the arithmetic and the comparison and the
write decision inside one tick.

The subtraction now happens one clock earlier, into a register.

**It is exactly equivalent, not approximately.** The position counter resets
whenever it reaches the packet length, and the region test needs position ≥ 36 —
so a value that lags by one clock has been correct for 35 clocks before anything
looks at it. The only moment the lag is visible is at a packet boundary, which
is precisely where the test is false anyway.

### Two honest caveats

**The clock-domain crossing is not fixed, it is currently passing.** Both of
this morning's headline failures were that crossing, and with the pressure taken
off elsewhere the fitter placed it better. Its slack depends on placement and
**can come back**. The decision about how to treat it properly is still yours,
on the docket.

**16 endpoints still fail**, so the tool still says FAIL overall. The remaining
ones are ordinary logic — the HPS arbiter reaching the command DMA's checksum
registers — and they are small (−0.423 down to −0.08).

### The part I got wrong earlier today

I told you 83.4 MHz this morning. That was the crossing, not the design. The
real figure was ~92, and it is now 95.9. I would rather say that plainly than
let the first number stand.

---

## 2026-08-24 (afternoon) — the composed shell FITS, and the 83 MHz I reported was wrong

### CORRECTION FIRST

Earlier today I wrote here that the graphics clock closes at **83.4 MHz**. That
number is real but it is **not the design's speed** — I attributed it to the
wrong thing before I could see the path detail, and the path detail was being
thrown away. Both are fixed now.

**The worst path is a clock-domain crossing that we deliberately left
unconstrained**, so the tool measures it as if the two clocks were related when
they are not. Excluding it:

| | |
| --- | --- |
| what I said this morning | 83.4 MHz |
| **the real worst logic path** | **≈92 MHz** |
| target | 100 MHz |

Still short — by 8%, not 17%. I would rather correct this the same day than let
a wrong number sit in the channel.

### The composed shell, built as one machine

All four stages succeeded. This is the measurement FRAMEBLIT step 8 was waiting
for, blocked on CMD.DMA since the 22nd.

| | composed shell | device |
| --- | ---: | ---: |
| logic (ALMs) | **7,442** | 41,910 — **17.8%** |
| registers | 9,926 | |
| memory | 114,688 bits in **13** M10Ks | 553 |
| multipliers | **0** | 112 |

**Area is not a problem.** The shell is under a fifth of the chip.

### The two things actually limiting the clock

**1. A counter crossing between clocks.** The video side counts starved cycles
in a 64-bit counter; the graphics side samples it. We chose *not* to tell the
tool to ignore that crossing, so it reports it as both the worst setup path and
the only hold failure. The crossing is guarded in hardware — there is a tripwire
that raises an error bit if the value ever moves while it is being sampled — so
this is a **measurement artefact, not a bug**.

But it does mean the timing verdict says FAIL forever, which hides real
failures behind a known one. **That is an owner call** and it is on the docket:
either tell the tool to ignore the crossing and characterise it separately, or
keep it visible and accept a permanently red verdict.

**2. The record framer, which is the real work.** The genuine worst paths are in
the glue that chops incoming command packets into records. It does, in a single
clock: two 32-bit comparisons against a subtraction, a 16-bit add-and-compare,
and on the result of all that, a **144-bit wide write** whose data is assembled
by a byte-insert at a position computed the same cycle.

That is the same shape as every block we have already fixed — a piece of
software written as one cycle of hardware. It is fixable the same way, and it
is now identified rather than suspected.

### Why I could not answer this an hour ago

The tool writes a full breakdown of every failing path. **Our harness threw all
of it away** and kept only the one-line summary, so the report could say "one
hold failure" and nothing on disk said *which one*. The only breakdowns left on
the machine were from the 22nd — a different build — and using those to answer a
question about today's would have been the exact mistake this project keeps
writing down.

Fixed: the breakdowns are kept with the result now. It changes nothing about
what is measured and everything about whether it can be acted on.

### Caveat, unchanged and important

This is the **shell** — 27 files: video scanout, command DMA, SDRAM, HPS bridge,
audio, debug. **It is not the renderer.** Terrain, Field, raster, texture and
geometry are not in this cone and have never been built as one machine at all.

---

## 2026-08-24 (midday) — a fit I had to throw away, and a proof that was never running

### I broke my own rule and it was worth finding

I edited a source file **while a Quartus fit was running**. Then I checked what
that actually does, and it is worse than sloppy: our fit harness hands Quartus
**the live files in the working folder**. Nothing is copied. So the fit measures
whatever the file says at the moment the tool happens to read it.

The timestamps: my edit at **10:33:06**, the tool's report at **10:34:47** —
101 seconds later, inside the window where it reads sources.

**So I killed the fit and threw it away.** It may well have been a perfectly
good measurement. The point is it could no longer be *shown* to be, and a number
we cannot show is worth nothing here — that is the same lesson as the
199 MHz that turned out to be 37.

The harness now takes a fingerprint of every source before, during and after,
and a run whose sources moved is marked contaminated and **cannot enter the
report**. The previous good measurement is kept rather than overwritten.

Getting the *test of that alarm* right took four tries. Three times it reported
"no contamination" and three times that was the test being broken, not the
alarm: once it edited too early, once it watched a **stale leftover folder**
(there were 36 of them), once it edited files that were not part of that build
at all. **A test that says "nothing found" is not evidence the detector works.**

### The Field engine's mantissa was doing work twice

One of the Field engine's slowest steps built **two 64-bit shifters in series**
to answer a question the step immediately before it had already answered. It is
now a direct read of the bits that were already in the right place — same
answer, verified against **50,000 random cases**, less hardware and a shorter
path. This is wave 1 of the Field plan.

### And a proof that has not run since the 23rd

Our formal proof for the Field sequencer — the one that proves the machine
**cannot hang**, which no ordinary test can show — was marked "pending" in the
ledger. I ran it.

**It did not even start.** It has been failing in one second since the register
rewrite on the 23rd, because the formal tool is stricter about declaration order
than our simulator is. Eleven complaints, all of them ordering, none of them a
change in behaviour.

**Nobody noticed because the entry already said "pending".** That is the part
worth keeping: a red already labelled *expected* stops being looked at. The
label was honest and it still hid a lane that was completely broken.

It now elaborates and is running — currently proving depth 21 of 172, nothing
violated so far. That run's wall time is the thing the ledger said was still
unmeasured, and I will report it when it lands.

---

## 2026-08-24 — the projector merge saved nothing, and found something better

### I predicted ~33 multipliers. The answer is zero, and I was wrong about why

I told you merging the two duplicate projectors was the largest remaining win —
33 multipliers of pure duplication. **The merge is done, proved correct, and saved
no multipliers at all.**

| | before | after |
| --- | ---: | ---: |
| geometry projector | 33 | **33** |
| terrain projector | 33 | **33** |
| the new shared core | — | 33 |

**A block that two others *instantiate* is not one they *share*.** Both callers
build their own copy of it, so the hardware is identical — the same arithmetic in
one file instead of two. My prediction confused code duplication with silicon
duplication, and they are not the same thing.

What the merge *did* buy is real but smaller: **one copy of the law instead of
two** (1,395 lines became 1,126), so the next change to projection cannot be made
to one projector and forgotten in the other. That is worth having. It is not 33
multipliers.

### Actually sharing one copy does not fit, and now we know why

The obvious next thought is to make them take turns on one core. **It does not
fit:** terrain projection alone already uses **99.5% of a frame's compute
budget.** Two users on one core is 106.7%. There is nothing to share *with*.

### Which uncovered the real problem, and it is a measurement one

The terrain projector's demand is filed in our budget file as **"270 patches per
frame"**. That number came from dividing a frame's budget by the cost of a patch.

**It is a capacity, filed as a demand.**

The consequence is bad: our new heatmap divides capacity by demand to rank what
is over-provisioned, and comparing capacity against itself gives a ratio of
about **6,000×**. So **the tool has been ranking the tightest block in the design
as the most wasteful one.** The true ratio is **1.0** — it is exactly full.

A unit mismatch made a saturated block look like the biggest opportunity on the
board. Anyone acting on that ranking would have tried to shrink a block with no
headroom whatsoever.

### So the priority has changed, and the change is well-founded

The terrain projector projects **triangle corners**, so a patch does **6,144
projections for 1,089 actual points** — each shared point re-projected about six
times.

Fixing that is not polish, it is the unlock:

| | share of a frame |
| --- | ---: |
| today, 6,144 projections per patch | **99.5%** |
| caching the 1,089 unique points | **17.6%** |

**A 5.6× reduction**, which is also what finally makes one shared core possible —
and only then does the merge that was just built pay for itself.

### What was done properly, and is worth noting

The agent **proved the two projectors were equivalent before merging them**,
rather than trusting the claim that they looked alike: 16,416 vertices compared
three ways against the software reference, zero mismatches. Its first attempt
scored 10 controls and caught only 9 — **it was blind to the exact boundary case
the law is written about**, and 12,300 random vertices never hit that one value.
It built a targeted test for it rather than accepting the pass.

And it confirmed neither caller changed behaviour: **1,080,000 cycles of every
output compared against a verbatim pre-merge copy, zero mismatches.**

---

## 2026-08-24 — the block that was twice the size of the chip now uses 0.7% of it

### The fix

The surface texture store was **229% of the whole chip**. It is now **0.7%**.

| | before | after |
| --- | ---: | ---: |
| memory it asked for | 131,072 bits | 131,072 bits |
| memory it actually got | **none** | **all of it** |
| flip-flops used instead | 131,258 | **170** |
| logic cells | 95,947 | **279** |

**A 344-fold reduction**, and nothing about what the block *does* changed. Every
one of those bits had been built out of flip-flops because of one detail in how
the code was written.

The cliff-edge block got the same treatment: **79% of the chip down to 18%.**

### What the detail was, and why I have to correct myself

I told you memory storage fails to become real memory for three reasons: reading
it without a clock, clearing it on reset, or writing only part of a word at a
time.

**Only the third one was actually wrong here.** The block's own notes said it had
been careful about the first two — and it was right. The single cause was writing
half a word at a time.

More precisely: the cliff block's table now works **while still being read
without a clock**. So that first "rule" is not a rule at all. **Writing part of a
word is the one that cannot be survived.**

The fix was to stop writing half-words — split the store into two arrays, one per
half. **Which is exactly the shape the software reference has always had.** The
hardware had been carrying a complication the software never needed.

### And a finding I would rather report than bury

**The tool we built yesterday to find these problems had been reporting this
block as healthy.**

It had no detector for the half-word case at all, and its reset detector was
reading the wrong half of the code — a subtlety of how the analysis tool
represents `if (reset)`. Two blind spots, one of them pointed directly at the
worst block in the repository.

Both are fixed and both now have tests that would catch the blindness returning.
But it is the same lesson this project keeps paying for: **the tool that checks
things is a thing that needs checking.**

### Where the design stands

| | |
| --- | ---: |
| multipliers | **134**, from 327 |
| ceiling | 85–90 |
| blocks over the chip's size | **none** — was two |
| biggest remaining block | the Field engine, 19% |

**Nothing is now too big to build.** That was not true yesterday.

### What is running

The **two duplicate projectors** — the same maths implemented twice, 33
multipliers each. Merging them is the single largest remaining win and needs no
decision from you, because duplication is duplication.

I have told it explicitly **not to trust** the audit's claim that the two are
identical. That claim is about the *shape* of the arithmetic, not about what the
blocks actually do. So the first job is to run both and compare every output —
and **if they disagree anywhere, that disagreement is a bug in one of them and is
worth more than the merge.**

---

## 2026-08-24 — the cheapest fix on the board turns out to be a number, not a rewrite

### There is a cliff at 27 bits, and most of our arithmetic is standing just past it

The calibration measured something nobody here knew: **a multiplication costs one
multiplier block if its inputs are 27 bits or narrower, and three if they are 28
to 33.** Nothing in between. It is a cliff, not a slope.

Almost all of this design's arithmetic uses **32-bit** inputs — one bit-width
band past the cliff, paying three times over.

I checked the rule against blocks we have actually measured before believing it:

| block | multiplications | input width | predicted | **measured** |
| --- | ---: | ---: | ---: | ---: |
| geometry projector | 11 | 32 | 33 | **33** |
| terrain projector | 11 | 32 | 33 | **33** |
| terrain normals | 6 | 33 | 18 | **18** |
| matrix engine | 3 | 32 | 9 | **9** |

**Exact, four times.** The rule predicts rather than describes.

### Which puts a very large number on the table

Thirteen blocks sit above the cliff. Together they hold **168 multipliers today**.
If their arithmetic fits in 27 bits, the same work costs **58**.

    a saving of 110 multipliers

For scale: we are at **134** and need to reach **85–90**. That is about 45 to
find. **This list holds more than twice that.**

### And it costs nothing

Everything we have done for two days has been rebuilding blocks — sharing
arithmetic, adding pipeline stages, sequencing loops. Each has taken about a day
and each changed how a block works.

**Narrowing a number changes nothing about how a block works.** No extra clock
cycles, no new state, no interface change, no rewrite. It needs one thing only:
**a proof that the smaller width is actually big enough.**

### The catch, stated plainly

**These are candidates, not winnings.** Each needs that proof, and some will fail
it:

* the creature-detail block's 64-bit path is a division and almost certainly
  cannot shrink — I have excluded it;
* **world coordinates may genuinely need 32 bits.** The projectors, terrain
  baking and culling all do position arithmetic, and whether 27 bits covers the
  world is a real question about how big the world is and how finely it is
  measured. **That one is partly yours** — it depends on the size of a Zhaozhou
  map and the precision the game needs, not on the hardware;
* the 58 assumes *every* multiplication shrinks, which will not happen.

Partial success is the realistic outcome, and partial success is still large.

### It also stacks with the work we were already going to do

Terrain normals is the clean example: **18 multipliers today, 6 if narrowed, and
about 3 after the sequencing we had already planned.** The two do not compete —
they multiply.

### So I have changed the recommended order

Before the next block rewrite, I would do a **width audit**: for each of these
thirteen, what is the true range of the numbers going in, and does 27 bits hold
them? That is answerable from the software reference and the world constants,
largely without running the chip tools at all.

If even half of the 110 is real, we reach the ceiling **without touching the two
duplicate projectors** — and if we remove that duplication too, the design lands
comfortably under it.

### Meanwhile

The surface texture store — the block at 229% of the chip because its memory
became flip-flops — is being fixed now.

---

## 2026-08-24 — the audit is done, and the biggest problem has no multipliers in it

### The headline nobody was looking for

We spent two days chasing multipliers. **The largest resource problem in the
repository is a block with none.**

`zhao_surface_sheet` — the surface texture store — declares 131,072 bits of
memory. **The chip tool inferred none of it.** Every bit became a flip-flop:

| | |
| --- | ---: |
| memory it asked for | 131,072 bits |
| memory it got | **0** |
| flip-flops instead | 131,258 |
| logic cells | **95,947** |
| **as a share of the whole chip** | **229%** |

**It does not fit. It is more than twice the device, on its own, and it has zero
multipliers — so it was invisible to every count we have made.** `FORGE.CLIFF` is
the same illness at 79% of the chip.

That is not a disaster so much as a relief: neither block is *doing* too much
work. They are both storing data in the wrong kind of silicon, and the fix is
a well-understood one.

### Because the audit turned that into a law, not a guess

The calibration ran 102 controlled experiments. Storage either becomes real
memory or it becomes a mountain of logic, and **three things independently push
it the wrong way**: reading without a clock, letting reset touch the array, or
using byte-enables. Any one is enough.

The penalty grows with size — **36× at 2 kbit, 502× at 32 kbit, 808× at the
worst point measured.** Byte-enables were the surprise: 65,536 bits cost **45,134
logic cells**, more than this chip has.

**So a block reporting zero memory is now a failing block, by measurement rather
than by opinion** — and both blocks above can be diagnosed without running the
hours-long test that previously just timed out.

### And a multiplier lever we have never had

    operand width      multipliers per product
    8 – 27 bits                 1
    28 – 33                     3
    40 – 48                     4
    64                          9

**Narrowing an operand from 32 bits to 27 turns one product from three
multipliers into one**, changing nothing else. Terrain normals' six products are
18 multipliers today; at 27 bits they are **6**, before any other work. That
single number explains an old mystery too — the same code once cost 28 and 18
depending only on how wide its inputs were. It was two bands apart.

### What the whole design actually looks like now

Every block is measured: **89 of 91**, against 41 of 94 two days ago. About
**200 multipliers** of arithmetic exist repo-wide, and the ranked list puts
**~75 of them** on three blocks, each derived from measured over-provisioning
rather than intuition.

The two projectors are confirmed as **byte-identical arithmetic** mapping to 33
multipliers each — the duplication is exact, not approximate.

### Honest limits, stated by the audit itself

**None of the 41 stored speed measurements describes today's code.** They will
have to be re-run. The slack figures the audit can now extract are **untested** —
no measurement has run since that code was written, so I will not quote one.

And it reported **seven of its own failures, five of them wrong predictions** —
including flagging a correct idiom as a defect, shipping a detector that could
never fire, and losing 97 measurements to a mistake it had fixed ten metres away
the same night. **That is the reason to trust the six findings above.**

### Where this leaves the work

Two blocks that do not fit the chip, and the fix for both is the same and is now
a measured rule. Three blocks holding ~75 multipliers of provable waste. And 84
blocks still with no idea how much work they must do per frame — which is the
next cheap win, because that number is what separates "expensive" from "wrong".

---

## 2026-08-24 — the audit found the hidden 33, and closed the coverage gap

### Every block is now measured, and the big prediction was exact

The scan finished: **89 of 90 blocks mapped**, where yesterday only 41 of 94 had
ever been measured at all.

**The geometry projector is 33 multipliers** — the same number, to the unit, as
the terrain projector we already measured at 33. Two implementations of one
piece of maths, and the second had never been measured because it had never been
run through the tool. Its own notes call the duplication "a cost, not a feature",
which turns out to be exactly 33 multipliers of cost.

**That single finding is worth more than yesterday's four rebuilds combined**,
because removing a duplicate is cheaper than making real work smaller.

Two more that nobody had flagged at all: the **triangle binning pipeline** at 21
and **terrain baking** at 17. And the pose decoder came in at **18**, inside the
14–18 that was predicted from reading the source.

### A number I am NOT going to quote at you

Adding up all 89 blocks gives 262, and **that figure is meaningless** — mapping
each block on its own counts shared parts several times over. The binning
pipeline contains the binner; three Field parts are inside the Field engine.

The audit has to work out what contains what before any total means anything. I
would rather tell you that than hand you a big number that looks like bad news
and is really just arithmetic done wrong.

**What stands: the 134 we measured properly, and the ~180 honest estimate.** The
scan confirms the estimate was the right size and found where the missing weight
was hiding.

### Why this run was worth doing instead of another rescue

Yesterday's method was: wait for a block's number to look alarming, then spend a
day on it. This run read **every** block in about four hours and produced a
ranked list — including two blocks nobody would have looked at for weeks.

It also tested itself honestly. It was set a deliberately falsifiable task: find
the two blocks we already knew were bad **without being told the answers.** That
is the difference between a tool that measures and a tool that confirms what you
already believe.

### Where the multipliers actually are, now that we can see them all

Biggest first: two projectors at 33 each, binning pipeline 21, pose decode 18,
terrain normals 18, terrain bake 17, culling 15, binner 12.

The four we rebuilt yesterday now sit near the bottom — Field engine 3, skinning
9, texture unit 6, surface stamping 0.

**The two projectors alone are more multipliers than everything we removed
yesterday.**

---

## 2026-08-23 (end of day) — 327 → 134 multipliers, and a change of method

### The day

| | multipliers |
| --- | ---: |
| this morning | **327** against a 112-multiplier chip |
| **tonight** | **134** |
| the ceiling | 85–90 |

Four blocks rebuilt, every number measured rather than estimated:

| block | before | after |
| --- | ---: | ---: |
| Field engine | 79 | **3** |
| creature skinning | 72 | **9** |
| surface stamping | 28 | **0** |
| texture unit | 28 | **6** |

Creature skinning also **meets its frame budget** — 124,514 vertices against the
120,000 you ruled.

### But I have to widen the estimate, not narrow it

The census covers **41 of 94** hardware files. Two significant blocks have never
been measured at all, and one of them almost certainly duplicates a block we
have already paid for:

* the **geometry projector** is absent from every report, and implements the
  same projection maths as the terrain projector we measured at 33 multipliers.
  Its own header calls the duplication "a cost, not a feature";
* the **pose maths** looks like another 14–18.

So the honest picture is **roughly 180 multipliers, not 134** — a design that has
to lose 95 more rather than 45. That sounds worse and I think it is better news
than it reads, because **about 50 of those are two copies of the same
arithmetic**, which is a cheaper thing to remove than real work.

### The change of method, which is the actual news

Every block so far was picked because its latest number looked alarming. That is
reactive, it has cost about a day each, and **the same seven mistakes keep
producing it**:

1. a placeholder "one per clock" becomes real parallel hardware before anyone
   counts how many items a frame needs;
2. software-shaped maths treated as one clock tick;
3. things that are memories built out of logic — **the Field engine uses none of
   the chip's 553 memories while 502 sit idle**;
4. the same arithmetic duplicated in neighbouring blocks;
5. measuring a block alone and calling it the speed it will have when connected;
6. confusing latency, rate and clock — only *work per second* answers anything;
7. tests that check the answers are right but cannot see that the hardware
   built was not the hardware described.

**All seven are detectable from the source before a chip tool finishes.** So the
next run is not another rescue — it is a tool that reads every block and produces
a ranked heatmap of which ones are lying, with hard gates so each mistake fails
the build rather than being rediscovered in a week.

The test I set for it: run it against the two blocks we already know are bad and
confirm they light up **without anyone telling it the answer.**

### Two corrections I owe you from today

**I gave you a speed number that was measuring a counter.** The texture unit was
reported at 199.72 MHz; it is 36.92. The timing file said nothing about paths
entering or leaving the block, so the tool silently ignored almost all of them —
and that block's arithmetic runs edge to edge. **Every speed figure taken before
this was fixed is suspect.** Multiplier counts are unaffected; they come from a
stage that never reads that file.

**And a proof I said had passed had not run.** The texture filter's mathematical
proof no longer completes — the rewrite made it a genuinely harder theorem rather
than the near-restatement it had been. What stands in its place is stronger, not
weaker: an exhaustive argument covering **every possible input**, rather than a
solver's word for it. But "the test harness needed no changes" is not the same
sentence as "the theorem still holds", and it was written down as though it were.

### Where the effort goes next

The two blocks that miss their targets both have specified fixes and neither is
mysterious. The **Field engine** needs to stop building memories out of logic.
The **texture unit** needs to accept more than one request at a time — it is
currently a calculator that can only hold one sum. Both are written up in full.

---

## 2026-08-23 (night, correction) — I gave you a speed figure that was measuring
## the wrong thing, and it affects every speed figure I have given you

### The correction

Two hours ago I told you the texture unit ran at **199.72 MHz, nearly twice what
it needs**, and concluded that of the four blocks measured, only the Field engine
was genuinely slow.

**That was wrong. The texture unit runs at 36.92 MHz** — it was holding the
shared clock to **37%** of its target, which is within noise of the 32% that
surface stamping was holding it to before it was fixed.

Same design, same tool, same chip. The only difference is what the timing file
was asked to check.

### What went wrong, because it is the same trap as yesterday

Yesterday's finding was that our timing file named clocks that did not exist, so
**no block was ever asked to hit a speed.** That is fixed.

What was **not** fixed: the file asks about the speed of paths *between
registers*, and says nothing about paths that begin or end at the block's
edge — so the timing tool **silently excludes them entirely.**

For the texture unit that is nearly the whole block. Its arithmetic runs from
its request inputs to its sample outputs; almost the only thing between two
registers is a counter. **So the 199.72 MHz I reported was the speed of the
counter.** The real arithmetic had never been timed at all — not slowly, not at
all — and the number I read as "twice its target" was measuring the one part of
the block nobody cares about.

Asked properly, the block is slow — but **the 37.0 ns "address generator" figure
I quoted was itself measured wrongly**, and I am correcting it in the same
breath. It came from applying the new constraints to a layout that had been
arranged with **no** such objective, so the tool had never once tried to make
those paths fast. It is an upper bound on an unoptimised layout, not a real
limit.

Measured properly — arranged *with* the objective present — the real worst path
is **21.4 ns**, and it runs from the format register through the decoder and the
filter to the sample output. The address generator improves and comes second.

**The reusable lesson: asking a timing question of a layout that was never
optimised for it does not give you that layout's speed.** It gives you a bound,
and bounds read like measurements if you are not careful.

### What this means for the other numbers

**Every speed figure in this project was measured the same way**, so all of them
are suspect until re-measured:

| block | what I told you | status |
| --- | ---: | --- |
| creature skinning | 89.65 MHz | **suspect** — needs re-measuring |
| surface stamping | 87.54 MHz | **suspect** — needs re-measuring |
| Field engine | 33.86 MHz | **suspect**, and likely the least affected |

How wrong each one is depends on how much of the block sits between registers
rather than at its edges. A deeply pipelined block will barely move; a block
that is mostly arithmetic from input to output — like the texture unit — moves by
5×.

**What is NOT affected: every multiplier count.** Those come from an earlier
stage that does not read the timing file at all. The census of **134** stands,
and so does every reduction behind it.

### The good part

**The tool is already fixed** — it now declares an arrival time on every input
and a required time on every output, so the paths that were invisible are
checked from here on. Validated against a real database before being trusted.

And the agent found this **while checking its own earlier conclusion rather than
moving on from it.** It had written "suspected, measured, and cleared" two hours
before, and went back to ask whether the measurement it used to clear the
suspicion was answering the question it was asked. It was not.

That is the eleventh time in two days that a measurement has been real and has
been measuring something other than what it was read as. I have stopped calling
these one-offs. **The rule stands: a green number from a tool nobody has watched
run is not evidence** — and now, explicitly, neither is a number from a tool
that was asked the wrong question.

### And the texture unit has a SECOND problem, independent of speed

It is not only slow. **It is also too slow in the other sense** — it takes six
clocks to produce one sample on the terrain path.

Even if it ran at the full 100 MHz it wants, six clocks per sample is **277,778
samples per frame against the ~850,000 a frame of terrain needs — a third of
it.** At its actual 36.11 MHz it manages about 102,000, an eighth.

So no amount of clock fixes it. The block accepts one request, does everything
for it, and only then accepts another. It is, as it stands, a calculator that
can only hold one sum at a time.

**The fix is to make it a conveyor**, and there is an unusually neat one
available: the texture cache already has four independent lanes, and the terrain
path uses only one. Putting a *new* texel lookup in lane 0 and an *older*
sample's palette lookup in lane 1 lets one complete sample finish every clock
after warm-up — **1.67 million per frame, twice what is needed**, rather than a
third of it.

Note also that the design's own existing proposal for this — two clocks per
sample — would have given 833,333 against a true demand of 829,440. **0.47%
headroom, and nothing at all for a cache miss.** That is the kind of number that
looks like success and is not.

### The good news, and it is real

**Nothing expensive is wrong with it.** Across the whole ladder — 28, 12, 6 and
3 multipliers — the speed barely moves. That is proof the multipliers were never
what made it slow. It cannot be rescued by adding arithmetic back, and it does
not need to be: it needs registers in the right places and a queue.

The texture unit is **28 → 6 multipliers**, and the six-lane choice turns out to
be not merely "within budget" but **the cheapest filter width that clears the
workload** — one lane would need 2.2 million filter cycles a frame against 1.67
million available. Census **160 → 134**.

---

## 2026-08-23 (late night) — the texture unit cost 28 multipliers and now costs
## 6; and the speed number I have been reporting for every block was wrong

### The good news first

The texture unit is the thing that reads pictures off memory and paints them
onto triangles. It was using **28 of the chip's 112 multipliers** — a quarter of
them, for one block.

**It now uses 6.** Every picture it produces is byte-for-byte identical to
before; this is the same arithmetic written differently.

What it was doing: blending four neighbouring dots of a picture needs four
weights, and each weight is itself a multiply, so the obvious way to write it is
eight multiplies per colour channel. Four channels, four copies of the circuit —
32 multiplies, twelve of them computing exactly the same numbers as their
neighbours.

Written the other way round it is **three multiplies a channel**, and then the
four channels can take turns through one pair of circuits instead of each having
its own. Same answer, every time, for every possible input — and that is
checked, not asserted: the two forms agree on **all 281 trillion possible
inputs**, established by an exhaustive argument in two halves (no internal value
can overflow its wires; and given that, the sum is exactly linear in the four
dots, so checking four cases per fraction pair settles every case).

**One thing there did go backwards and I am flagging it.** The machine-checked
proof that used to guard this arithmetic no longer finishes — it ran 55 minutes
without an answer, where the old form took 12. Neither the tool nor the block is
wrong: the old code was written in *exactly* the shape the proof was written in,
so the check was nearly trivial; the new code is a genuinely different shape that
happens to compute the same thing, which is far harder to check mechanically. I
published "the proof still passes" in three places before noticing, on the
strength of the proof *harness* needing no edit — which is a different claim.
Corrected everywhere, and the exhaustive argument above is what carries it for
now.

Running total: the multiplier demand across measured blocks is now **134**
against the chip's 112, down from 327 this morning.

### The bad news, and it is worse than the good news is good

**Every speed number in the per-block reports has been measuring the wrong
thing, and I only found out because one number looked absurd.**

To ask "how fast can this block run?", you have to tell the tool which paths to
measure. We were telling it about the clock and nothing else — so it measured
only the paths that start and end inside the block, and ignored everything
running from the block's inputs or to its outputs. For a block like this one,
whose entire job sits *between* its inputs and outputs, that is almost the whole
block.

The texture unit reported **199.72 MHz**. I checked which path produced it. It
was the **counter that tallies how many texels the block has drawn** — a piece
of bookkeeping. Its actual arithmetic appeared in no measurement at all.

With the tool told about the input and output paths, the same unchanged block
reports **36.92 MHz**. The console is designed around 100 MHz.

That is the same disease SURFACE.STAMP had last week (32 MHz), which I reported
as found-and-fixed. **I also reported, this afternoon, that the texture unit was
clear of it — because I had re-measured and got 199.72 MHz.** That reading was
wrong, and the reason it was wrong is the paragraph above.

The measurement tool is fixed. What it means:

* **Every per-block speed number recorded before tonight is suspect**, in the
  same way and for the same reason as the batch that was invalidated a few days
  ago. Re-measuring them is a job of its own and I have not done it.
* **This block does not run fast enough**, and cutting its multipliers did not
  change that — 36.92 before, 36.11 after. The slow part is a different piece of
  it, and the fix is a known one (a pipeline stage) that I have specified but not
  built, because it is the same change as the throughput fix below.

### And a third thing, which is smaller but the same shape

The block is supposed to produce about **850,000 texture reads a frame** — a
figure derived from Sacrifice's own terrain, which layers three pictures on every
patch of ground. **It produces about a third of that.**

Nothing had ever measured it. The ledger said "1 sample per clock", the block's
own contract said that was not met, and both were prose. There is now a test that
measures it and fails if it moves.

None of this is a regression. All three were true this morning; two of them were
invisible and one was written down and unchecked.

---

## 2026-08-23 (late night) — we know why the Field engine is slow, and the fix
## is to use the memory we already have and aren't touching

### The finding

The Field engine uses **none** of the chip's 553 block memories. Not a few —
none. Zero. And it spends **8,901 logic cells**, a large part of them on things
that are memories in everything but name:

* a 64-entry register file built out of raw flip-flops, read through several
  enormous 64-way selectors — the block's own documentation already names those
  selectors as its biggest cost;
* three constant lookup tables written as giant `if/else` trees in logic;
* a sine table **built twice**, because the maths needs two entries from it at
  once.

Meanwhile the whole design uses **51 of 553** block memories. **502 sit idle.**

The chip's block memories are exactly the right home for all of that: they are
synchronous, dual-ported, and can ship pre-loaded with constant data.

> **The Field engine is starving for speed while refusing to spend the one
> resource it has in abundance.**

Six of those 502 would hold everything listed above.

### Why this is good news

This morning 8.59 MHz looked like it might mean the Field instruction set was
too ambitious for the chip. It does not. The multiplier rewrite (79 → 3) was
right, the loop fix (8.59 → 33.86 MHz) was right, and what they have exposed is
that the engine is still shaped like **software translated literally into
hardware** rather than like a small processor.

The plan is to keep the arithmetic exactly as it is and rebuild the plumbing
around it: memories where memories belong, and results parked in registers
instead of racing through several stages of logic in a single tick. Estimated
outcome — and these are estimates, not measurements — **3,500–5,000 logic cells
(from 7,750), still 3 multipliers, and 100–120 MHz.**

### Why it can afford to be slower per instruction

Because the trade is measured in **real time**, not clock ticks:

| | today | proposed |
| --- | ---: | ---: |
| a simple operation | 6 ticks at 33.86 MHz = **5.6 M/s** | 7 ticks at 100 MHz = **14.3 M/s** |
| a normalise | 67 ticks = **0.51 M/s** | ~80 ticks = **1.25 M/s** |

**Even taking eight ticks instead of six, the rebuilt engine would do more than
twice the real work.** This is the same lesson that turned creature skinning
from failing into passing: the number that matters is work per second, never
megahertz.

### The question I cannot answer without you, eventually

**Nobody has ever worked out how much Field work a frame actually needs.**
Several of its five uses still claim "one instruction per clock" — the same
placeholder that had this console asking for three times the multipliers it has.

The sum each one owes is simply: *how often it runs* × *how long its program is*
× *how many ticks each instruction takes*. To show why it matters, a plausible
guess for just one of the five uses:

    120,000 deformed vertices x 8 instructions x 8 ticks = 7.7 million ticks

against **1.7 million ticks in a frame**. That is four and a half times over on
one use out of five.

If that turns out to be true, it does **not** mean the design failed. It means
one engine was the wrong quantity. Three copies of the same verified engine
would be **nine multipliers** — the original single engine used seventy-nine.
That stays faithful to your "one engine, five profiles" ruling: identical
instances, not five different designs.

**Nothing is blocked on this today.** But when you want to think about it, the
question is: roughly how much procedural maths does a frame of Zhaozhou need —
craters, deformation, particle forces, formations, scars?

### Elsewhere

The texture unit's *existing* design was just measured properly for the first
time: **199.72 MHz**, nearly twice what it needs. So of the four blocks now
measured against a real clock, three are comfortably fast and only the Field
engine is genuinely slow. That is a much better picture than this morning's.

Multiplier count still **160**, down from 327.

---

## 2026-08-23 (late) — surface stamping: 28 multipliers to zero, and 160 total

### The block needs no multipliers at all

| | before | after |
| --- | ---: | ---: |
| multipliers | 28 | **0** |
| logic cells | 947 | 993 |
| speed | **32.33 MHz** | **87.54 MHz** |

**Both measured the same way**, which matters — the "before" number is a fresh
re-measurement of the old design under the corrected timing setup, not the old
optimistic figure. So this is a fair comparison rather than a flattering one:
**28 multipliers gone, 46 logic cells added, and 2.7× faster.**

This was the first block sized from **evidence rather than a guess**, and the
evidence was Sacrifice itself. The prediction was "0–2 multipliers". The answer
is zero, at every setting of the design dial.

The reasoning that got there, since it is the template for the rest: a big spell
scar marks about 36,864 texels, and it happens **once per impact, not once per
frame**. Even a heavy barrage needs ~6,000 texels per frame from a block built
for 1,666,667. And the original game composited its scars through a **lookup
table**, not arithmetic — so there was never a reason for a multiplier farm to
exist here at all.

### Where the campaign stands

| | multipliers |
| --- | ---: |
| this morning | **327** against a 112-multiplier chip |
| **now** | **160** |
| if the next two hit their derived targets | ~124 |
| the ceiling | **85–90** |

Today removed **167 multipliers**, every one measured rather than estimated:
the Field engine 79 → 3, creature skinning 72 → 9, surface stamping 28 → 0.

Still to go: terrain projection 33, the texture unit 28 (derived target 6–9),
terrain normals 18 (derived target 1–2), culling 15, the binner 12.

### Speed, which is now the harder problem

| block | speed |
| --- | ---: |
| creature skinning | **89.65 MHz** — meets its budget |
| surface stamping | **87.54 MHz** |
| Field engine | **33.86 MHz** — still 3× short |

Three blocks measured, two of them comfortably fast. **The design is not
uniformly slow** — that is the most useful thing we have learned, because this
morning it looked like it might be. Thirty-seven blocks still have no speed
figure at all.

### One thing I found by checking rather than by being bitten

Our texture unit can only address textures whose dimensions are **powers of
two**. Sacrifice's creature textures are 256 wide with **arbitrary heights up to
799** — only 13% are power-of-two in both directions, because each is a vertical
strip of body parts stacked together.

That is not a hardware fault. The restriction is what makes texture addressing a
shift instead of a multiply, and removing it would add multipliers to the very
block we are about to cut from 28 to 6–9.

The fix belongs in the asset pipeline, and it is cheap: **split those strips at
body-part boundaries into power-of-two tiles.** That is a repack, not a
resample — no pixel is altered and nothing is lost.

**This is the third time today** a piece of hardware turned out to be efficient
*because* it assumes something about the content, with the assumption written
down inside the block and nowhere the asset importer would look. The other two
are both in creature skinning: bone weights must add up to a fixed total, and no
vertex may use more than two bones (Sacrifice uses three for 2.5% of vertices,
at the shoulders and hips).

None of these is urgent. But when the first real creature looks subtly wrong,
the search will start in the hardware — which is correct — instead of in the
importer, which was never told. So they are now collected in one place.

---

## 2026-08-23 (night) — the answer on the Field engine: 8.59 → 33.86 MHz

### The number you were waiting for

This morning the Field engine measured **8.59 MHz** against a 100 MHz target,
and the cause was one piece of arithmetic written as a 128-step chain. The fix
is in, and it is bit-identical to what it replaced.

| | before | after |
| --- | ---: | ---: |
| speed | 8.59 MHz | **33.86 MHz** |
| worst path overshoot | −106.4 ns | **−19.5 ns** |
| **total overshoot across all paths** | −2,122,226 ns | **−45,290 ns** |
| logic area | 10,615 | **7,750** cells |
| multipliers | 79 | **3** |

**3.94× faster from one rewrite.** And the area fell again — the 128-step chain
was costing about 1,151 logic cells as well as the clock. Overall this block is
**27% smaller and 96% cheaper in multipliers** than it started.

### But be clear: this is not "fixed"

**33.86 MHz still misses 100 MHz by three times.** The two possibilities we
framed this morning were *"maybe 28 MHz, not 100"* or *"everything else is fine
and we're basically done"*. **It is squarely the first.** There is a second slow
path and it has not been named yet.

So the honest reading: the loop really was the dominant problem, not a symptom —
a 3.94× return proves that — but the Field engine needs at least one more round.
**This is a campaign, not a one-off bug.** I would rather say that now than
imply we are closer than we are.

### One detail worth your attention, because it tells us what kind of problem this is

The worst single path improved 5.4×, but the **total** overshoot across every
failing path improved **47×**.

That asymmetry is informative. If the old design had contained one freak slow
path, fixing it would have moved the worst-case number and left the total
roughly alone. Instead a whole *population* of paths improved together — which
is exactly what you would expect from 128 stacked stages feeding several
destinations at once. **It means the fix removed a systemic cost rather than
clipping an outlier**, and it is the reason the remaining work is likely to be
ordinary rather than another crisis.

### Read it against the other block we measured

| block | speed | meets its own budget? |
| --- | ---: | --- |
| creature skinning | **89.65 MHz** | **yes** — 124,514 vertices/frame against 120,000 |
| Field engine | **33.86 MHz** | not yet |

Two blocks, both fixed today, landing 2.6× apart. That is genuinely reassuring:
**the design is not uniformly slow.** One block is essentially there; the other
needs another pass. A scan of the whole design for the same 128-step mistake
found only one other candidate, and it is not in either of these.

### What happens next

The critical-path query is running on the fixed design and will name the new
worst wire in the same form that made the first one a one-line fix. **Nobody is
calling 33.86 good or bad news until we can say what is now limiting it** —
which is the discipline that produced every real result today.

---

## 2026-08-23 (evening) — creature skinning is finished, and Sacrifice answered your three questions

### Skinning: 72 multipliers to 9, and it now meets its own budget

| | before | after |
| --- | ---: | ---: |
| multipliers | 72 | **9** |
| speed | never measured | **89.65 MHz** |
| clocks per vertex | 1 (claimed) | 12 |
| **vertices per frame** | — | **124,514** |

You required 120,000. It delivers **124,514 — 103.8%**. Earlier today it was at
97,417 and failing; splitting the slow arithmetic across two more steps bought
31 MHz for **38 logic cells and no extra multipliers**.

The thing that made this work is worth keeping: the test was never "how fast is
the clock" but **"how many vertices per frame"**. A fix that adds steps can win,
and a fix that buys megahertz can lose. Both numbers get reported now.

**We also measured the width dial at all three settings, and it saved us from a
bad trade:**

| width | multipliers | speed | vertices/frame |
| ---: | ---: | ---: | ---: |
| 1 | 3 | 56 MHz | 38,965 — **fails** |
| **3 (shipping)** | **9** | **89.65 MHz** | **124,514 — passes** |
| 6 | 18 | 84.61 MHz | 141,017 — passes |

Doubling to 18 multipliers buys 13% more throughput **and makes the clock
worse.** 16% of the chip's multiplier budget for 13% more of something we
already have is not a trade worth making. A single measurement could never have
shown that — this is what "measure two or three points" was for.

**One thing left open:** at 89.65 MHz this block would cap the shared GPU clock
below 100 MHz. Margin is 3.8%. Thin, but real, and honestly reported.

### Your three questions: answered from Sacrifice itself

You pointed me at the game rather than answering, which was the better move —
these are evidence now, not policy. Full working in `docs/OWNER_DOCKET.md`.

**And your 120,000-vertex ruling checks out independently.** 93 creature models
measured: median 2,951 vertices. The real limit on army size is the soul
economy — 4 to 12 souls per wizard across all 32 shipped maps, creatures costing
1 to 5 — which puts a realistic battle at **40 to 60 characters**. That is
118,000 to 177,000 vertices. Your number is about 41 creatures at typical
detail. It was well chosen.

**The ratio that governs everything else:** Sacrifice ran at 800×600. We render
**19% of its pixels**. So roughly five times the per-pixel budget to spend on
more stuff, exactly as you guessed.

| block | multipliers now | derived target | why |
| --- | ---: | ---: | --- |
| surface stamping | 28 | **0–2** | a big spell scar marks 36,864 texels **once per impact**. A heavy barrage needs 6,144 per frame; it is built for 1,666,667. **Over-built about 270×** — and the original did it with a lookup table, no arithmetic at all |
| texture unit | 28 | **6–9** | terrain needs three samples per pixel; at our resolution that is **one sample every two clocks**, not one per clock |
| terrain normals | 18 | **1–2** | craters are tiny — 9 to 25 grid points each. Rebuilding the **entire** landscape every second would still be 0.07% of what the block is built for |

**If those land: about 124 multipliers**, before three more blocks are even
touched. The ceiling of 85–90 is reachable.

### Two things that are wrong and need your eye eventually

**Sacrifice skins to three bones. We built for two.** Measured across 317,234
vertices: 65% use one bone, 32% use two, and **2.5% use three**. Ours handles
97.5% exactly and clips the rest — and the clipped ones are precisely the seam
vertices at shoulders, hips and necks, where a mistake shows most. Cheapest fix
is probably a rare second pass for that 1-in-40. **Not urgent, but it is a
decision, not an oversight.**

**Sacrifice's bone weights do not add up to a fixed total** — and our clever
arithmetic shortcut, the one that halved the multiplier count, assumes they do.
Fine if our own asset pipeline normalises them on import, but that is now a
written requirement rather than a silent assumption.

And one for whoever builds the texture unit: **character textures are 256 wide
with arbitrary heights up to 799** — only 13% are the neat power-of-two sizes
hardware usually assumes. Each is a vertical strip. A texture unit built for
square power-of-two art breaks on most of the creatures.

---

## 2026-08-23 (late afternoon) — the mutation tests were checking nothing

### Read this one first

Our strongest quality tool is the **mutation sweep**: deliberately break the
hardware in one small way, then confirm a test notices. It is how we know the
tests are worth anything.

**In a fresh copy of the repository, it was testing nothing at all — and
reporting success.**

The mutant list is read from a text file. On a fresh checkout on Windows those
lines gain an invisible extra character at the end, the pattern that reads them
matched nothing, and the tool printed

> linted 0 mutants, 0 do not build

and **exited successfully.** A clean pass over an empty set. Every sweep run
that way was a green tick for work never done.

Found by an agent running a sweep in a fresh worktree for the first time,
because that is what the rules require. Fixed two ways, deliberately — the file
endings are now pinned so it cannot recur, **and** the tool now refuses to
score fewer than two mutants, on the principle that a guard which checks nothing
must fail rather than pass.

This is the same shape as the timing defect from yesterday: a tool doing nothing
while reporting success, with no symptom except a number that never moved. That
is now three times in two days, so I have stopped treating it as bad luck.
**A green result from a tool nobody has watched run is not evidence.**

### And then I checked whether it had already damaged anything. It had not.

The obvious next question — *how many of our recorded test scores are worthless?*
— deserved an answer rather than a reassurance, so I went through every sweep
result in the repository looking for the empty-set signature.

**Every recorded score parsed a real set:**

| where | what it recorded |
| --- | --- |
| mesh-fetch cull sweep | linted **32** mutants, 32 accounted, 30 caught |
| creature skinning sweep | linted **28** mutants at three settings, 0 failed to build |
| Field engine sweep | 33 attempted, 33 accounted, 30 caught |

The only `linted 0` lines anywhere are in the write-ups **describing** the bug,
including the one above. Nothing to re-run, nothing withdrawn.

The reason the blast radius is zero is worth keeping: the fault only bites in a
**fresh** copy of the repository, and running sweeps in a fresh copy is a rule
you introduced recently. So the very first run under the new rule hit it, which
is the best possible time to find a fault of this kind — before it had a history
to poison.

### Creature skinning: the 58 MHz problem is diagnosed exactly

Not guessed. The timing tool named one wire, and **all 200 of the worst paths
end at the same place**:

    from  the blend-walk row counter
    to    the output register for row 1
          10 levels of logic, 17.6 ns of delay against a 10 ns budget

Two things that follow, both of which save wasted work:

* **Over half the delay is real logic, not wiring.** So no amount of tuning the
  chip tool will fix it — the arithmetic has to be split across more steps.
* **Neither end of the path is an artefact of measuring the block on its own.**
  That was a genuine worry and it is now ruled out, so no test scaffolding is
  needed first.

The suspected alternative — the multiplier accumulation — was **exonerated by
experiment rather than by argument.** Building a 1-multiplier-wide version
leaves the suspect path untouched while collapsing the accumulation, and the
speed did not move (58.45 → 56.11). That is what a controlled experiment looks
like, and this project has previously been burned by skipping exactly that.

The repair is specified and being implemented: split the final blend across two
more steps. That costs two clocks and the budget has room for three — **the
block needs 86.4 MHz at twelve steps, and would still pass at thirteen.** The
number to judge it by is vertices per frame, not megahertz.

### The dial has a second measured setting

The width dial now has two real points on record:

| setting | multipliers | logic | speed |
| --- | ---: | ---: | ---: |
| 3 wide (shipping) | **9** | 2,187 | 58.45 MHz |
| 1 wide | **3** | 1,530 | 56.11 MHz |

The 1-wide point is kept precisely **because it fails** the vertex budget. A
range with no failing end does not show where the wall is. The third point,
6-wide, is not measured yet.

And one unglamorous but real benefit: **a mutant was caught only by the 1-wide
build.** At the wider settings that piece of logic collapses into a single step
and the test cannot see it. So building several settings is extra *coverage*,
not just extra data.

### Where the multiplier count stands

**188** against a chip with 112, from 327 this morning. The ceiling is 85–90.

Three of the remaining blocks — 74 multipliers between them — cannot be sized
because nobody has ever said how much work they must do per frame. Three
questions for you are on the docket; none is urgent.

---

## 2026-08-23 (afternoon) — 188 multipliers, and the second speed number

### The multiplier problem is nearly solved

| | multipliers |
| --- | ---: |
| this morning | 327 |
| after the Field engine | 251 |
| **now, after creature skinning** | **188** |
| the policy ceiling | 85–90 |

Creature skinning went **72 → 9** against a target of 12–18. It was 64% of the
whole chip; it is now 8%. That is 139 multipliers removed today, measured, from
a design that wanted three times what the chip has.

**One honest correction to something I told you earlier.** I have been saying
that shrinking these blocks also shrank their logic every time — that sequencing
does not trade area for multipliers. That was true of the three blocks it had
been true of, and it is **not** true here: creature skinning's logic went
1,801 → 2,187, up 21%. The earlier blocks were wide parallel datapaths that
collapsed into one. This block was already lean, and its size genuinely *was*
the multipliers, so sequencing it had to add bookkeeping instead. The trade is
still overwhelmingly worth it — 63 multipliers is 56% of the chip's multiplier
budget, 386 logic cells is 0.9% of its logic budget — but the general claim was
too broad and I am withdrawing it.

### The second speed measurement, and it is not good enough

| block | speed | target |
| --- | ---: | ---: |
| Field engine | 8.59 MHz | 100 |
| **creature skinning** | **58.45 MHz** | 100 |

**58 MHz is not acceptable and it is being diagnosed now.** At that clock the
block serves about 97,000 skinned vertices per frame against the 120,000 you
ruled. So it misses its own budget, and the multiplier win does not matter if
the clock cannot deliver the vertices.

The diagnosis run is already set up: the timing tool will be asked to name the
exact wire, in the same form that turned the Field engine's problem from
alarming into a one-line fix. There is a prime suspect — sequencing this block
added six very wide accumulators, and if a multiply, a wide addition and a
saturation all happen between two clock ticks, that is about the delay observed.
**But nothing will be changed until the tool confirms it.** Acting on a
plausible cause instead of a measured one is the single most expensive habit
this project has had.

### What two measurements tell us that one did not

Both blocks measured so far miss the clock. That is the bad reading.

The good reading is that **they miss it in completely different regimes** —
8.59 against 58.45 is not one systemic fault, it is two separate problems. The
Field engine's cause was a piece of arithmetic written as a 128-step chain; a
scan of the entire design found only one other place with that shape, and
creature skinning was not one of them. So the second block's problem is
ordinary arithmetic depth, which is the routine kind.

It remains true that **thirty-nine other blocks still have no speed figure at
all** and that nobody should be surprised by what they say.

### Widescreen is on the docket

Fully written up in `docs/OWNER_DOCKET.md`, not implemented, and deliberately
not interrupting this work.

The short version: a genuine 16:9 square-pixel mode at **384×216**, which is an
exact 5× enlargement to a 1080p television — every console pixel becomes a
perfect 5×5 block, and scanlines land identically on every row. It stores into a
384×224 canvas so the tile machinery stays exact, and displays a centred 216-row
window, which turns out to cost **fewer** tiles and **less** memory traffic than
the current mode. The camera is genuinely 16:9 rather than a cropped 16:10
picture.

One decision for you when convenient, and it is not urgent: that mode, versus
simply stretching the existing picture to 16:9.

---

## 2026-08-23 (midday) — the multiplier count is really down, and one decision needs you

### The number that was wrong all morning is now right

Main claimed the design wanted **327** multipliers against a chip that has 112.
It now says **251**, and that is not a re-estimate — it is the Field engine's
real measurement finally reaching the report. For most of the morning the code
in main contained the 3-multiplier engine while the report still said 79, which
is worse than a stale number, because the two disagreed.

| | multipliers |
| --- | ---: |
| this morning | 327 |
| **now, measured** | **251** |
| once creature skinning lands | ~194 expected |
| the policy ceiling | 85–90 |

### One block in this entire design has a real speed number

That block is the Field engine, and the number is **8.59 MHz** against a target
of 100. Everything else — forty other measured blocks — still has no speed
figure at all, because until yesterday the tool was never asked for one.

The fix for the Field engine's slowness **is written and merged**. It is
bit-identical to what it replaced: same answers, same clock counts, 419 directed
plus 90,000 random cases agreeing, and the latency table unchanged to the byte.
The measurement that says whether it worked is running now.

**That measurement is the most important number in the project right now.** If
it comes back near 100 MHz, this was one embarrassing bug. If it comes back at
20 or 30, there is a queue of them and we have a timing campaign ahead. Nobody
can tell which from here.

To narrow where to look, I scanned the whole design's source for the same
mistake — arithmetic written as a long chain the chip has to walk end to end in
one tick. **Only one other candidate exists**, in the rasteriser, and I have
deliberately not touched it: three things in this project have been "fixed" on
suspicion and two of them were not broken. It gets measured first.

### Creature skinning was over-provisioned by 13.9x

The block uses 72 multipliers — 64% of the whole chip for one stage. It had
never been queued for reduction, on the reasonable argument that vertices are
the fastest-moving thing in the pipeline so the hardware might be earned.

Your 120,000-vertices-per-frame number settled it. That allows 13.88 clocks per
vertex; the block does 18 multiplies per vertex; so the honest requirement is
**1.30 multipliers** and it was built with eighteen.

It is being rebuilt with the width as a **dial** — 1, 3 and 6 — with all three
actually built and tested, rather than picking one and declaring it right. The
1-wide setting is kept deliberately **because it fails**, at 75,757 vertices per
frame. A range with no failing end does not show you where the wall is.

### THE DECISION — the skinning reference quietly truncates, and I did not fix it

Full write-up in `docs/OWNER_DOCKET.md`. Short version:

The reference renderer computes a skinned vertex in very wide arithmetic, which
is correct and deliberate — then hands the result to a function that only takes
half that width. Anything too large wraps. At the extreme it turns a maximum
positive value into a maximum **negative** one.

Three things say it is an accident, not a choice: the function's own comment is
careful about exactly this, a wider version of it exists twenty lines below, and
nothing depends on the wrap.

**Nobody saw it because no real pose can reach it.** Across 24,000 random
pose-range coordinates, zero left the region where both answers agree. It needs
a creature bent into a shape none of them take.

It is also **not** a regression — the old hardware diverged from the reference in
exactly the same places, for as long as both have existed. Nothing had ever
compared them there.

I did not fix it, because all three fixes change arithmetic that either the
reference pictures or the console were built on:

* **fix the reference** — one line, correct, and it changes the function every
  shipped picture was skinned with. Only in the unreachable region — but
  "unreachable" is a measurement over the poses that exist today, not a proof;
* **make the hardware copy the truncation** — hardware then matches the
  reference exactly, at the cost of the console deliberately reproducing a C++
  accident in silicon;
* **leave the boundary pinned where it is** — which is the committed state: the
  test checks the reference wherever the reference is well defined, and says so
  by name where it cannot.

I lean toward the first, but it is your call, and it is not urgent.

### Housekeeping worth one line each

**Runs stopped being losable.** The tool that creates them lived outside the
repository, which the notes already blamed for four forgotten days — and the fix
that had been prescribed, copying finished runs in by hand, was itself being
forgotten. The tool now lives inside the repo and writes straight into the
tracked folder.

**The tests are green except one, and that one is honest.** 259 of 262. The
failure is the ledger refusing to let the Field engine claim "verified" while
its proof is still running. That is the gate doing its job, and it clears when
the proof finishes.

**An eighth Quartus trap is on record** — a piece of syntax three other tools
accept and Quartus rejects outright.

---

## 2026-08-23 (later) — the multipliers were never the problem with speed

### What we can now see, and could not before

Yesterday's finding was that no block had ever been asked to hit a speed. That
is fixed, and the first block has now been measured properly on both sides of a
big rewrite. The result is not what the multiplier work predicted.

| the Field engine | multipliers | speed |
| --- | ---: | ---: |
| before the rewrite | 79 | **7.72 MHz** |
| after the rewrite | **3** | **8.59 MHz** |

The target is 100 MHz. **It is twelve times too slow, and it was twelve times
too slow before as well.** Removing 76 of 79 multipliers bought 11%.

So the multiplier campaign was necessary — the design still wants three times
the multipliers the chip has — but it was never going to fix speed, because
multipliers were not what was slow.

### What is slow, named exactly

The timing tool pointed at one wire and all three of the worst paths are that
same wire, with **78 levels of logic in a single clock tick**. It is inside the
normalise block: a piece of arithmetic written as two sixty-four-step loops that
the hardware has to lay out end to end, 128 stages deep, all inside one tick.

To be exact about what is proven and what is inferred: there is no other route
through that block from the input to the output, so the loops are certainly on
the path and certainly **dominate** it — but nobody walked all 78 cells, and the
rounding step at the end is worth perhaps 8 to 10 of them. "Dominated by the
loops" is the honest phrasing; "is the loops" would be one step further than the
evidence goes.

The fix is cheap and already exists a few lines away — a neighbouring block does
the same job the right way, in a handful of steps instead of 128. **It costs
nothing.** There are two ways to do it: one parks the intermediate result in an
extra step, and costs one clock on an operation that has thirteen clocks of
slack; the other simply counts the leading zeros the way the neighbouring block
already does, and costs **no clocks at all**. Take the second. I originally
wrote this as "costs one clock", which invites the question "is it worth a
clock?" — and the good answer does not cost one.

**We deliberately did not do it in this pass.** It is a different change, and it
would invalidate the mutation sweep and both chip measurements that were just
taken. It is on the docket with the measurement attached, to be picked up with
the next piece of Field work rather than as a special trip.

### What we cannot tell you yet, and it is the whole question

Killing the worst path does not give you the target — it reveals the
second-worst path. After this fix the engine could land at 30 MHz with another
monster behind it, or at 90 MHz with nothing much behind it. **There is no way
to know until it is re-measured**, and that single number decides whether this
is one embarrassing bug or a timing-rearchitecture campaign across the whole
design. It is the most informative measurement available to us right now, which
is why it has moved to the front of the queue.

### The part you should actually weigh

Every one of the 47 block measurements was taken with no speed objective.
**We now have good reason to think other blocks are also far from closing, and
no evidence either way until they are re-measured.**

And the area column is worse off than "missing a speed column" suggests. With no
speed to hit, the tool spends its whole effort making the design *small*. So
those 47 area numbers are not neutral — they are the **optimistic end** for
designs that will later be asked to run fast. The census is not merely missing a
column; the column it does have was measured against the wrong objective.

That is a new line of work, not a continuation of the current one. It does not
change the multiplier plan; it sits beside it.

One thing that is NOT affected: **the composed shell still measures ~95.5 MHz**
and that number was always honest, because the shell has its own correct timing
file. The heavy arithmetic engines are not in the shell yet. The gap is between
the blocks and the machine they are going to be dropped into.

### Where the multiplier count stands

| | multipliers |
| --- | ---: |
| measured today, 41 of 47 blocks | **327** against 112 |
| once the Field engine's new number is recorded | **251** |
| with the creature-skinning block at its target | **~194** |
| policy ceiling | **85–90** |

Biggest remaining: creature skinning 72, terrain projection 33, surface stamp
28, texture unit 28, terrain normals 18, culling 15, binner 12. Creature
skinning is being worked now.

### Two smaller things

**A measurement that had been destroyed is back.** The creature-detail block's
row said "failed" with no numbers, because a later measurement I killed had
overwritten a good one. It was recovered from history rather than retyped — the
tool's own bytes, at a commit where the design is provably unchanged — and it
restores the 18 → 6 result to the report. Note that this made the total go
**up**, 321 to 327: the block had been silently counted as zero.

**The repo would not build.** Three separate pieces of stale build state, none
of them design faults, one of which made a completely correct file list report a
missing file. The correct build sequence had only ever been written inside a
note field in a config file; it is now in `docs/BUILD.md`.

---

## 2026-08-23 — every speed measurement I have given you was never taken

### The defect

When I measure one block on its own, I hand the chip tool a file saying *"this
clock must run at 100 MHz — now place the design so it does."*

That file names three clocks: `gpu_clk`, `vid_clk`, `audio_clk`. Those are the
names used at the **top level** of the console. Individual blocks call their
clock simply `clk`. Sixty-three of the design's seventy-one clock connections
are named `clk`.

So the tool looked for something to constrain, found nothing, and said so —
three times, in **every measurement run this project has ever done**:

> Ignored create_clock: Argument <targets> is an empty collection

**No block was ever asked to hit a speed.** Not one of the 47 measurements.

### What that does to the numbers

| what I told you | status |
| --- | --- |
| multiplier counts | **still good** — those come from an earlier stage that does not use the timing file |
| logic area | **optimistic** — measured with no speed pressure; meeting a real speed usually costs more area |
| speed (MHz) | **not slow measurements. Not measurements at all.** |

The multiplier story — 327 wanted against 112, and the reductions to 6, to 3,
and the Field engine to 4 — is the part that survives, which is fortunate,
because it is the part driving the current work.

The one real casualty: **nobody knows how fast any individual block is.** The
old Field engine showed 7.75 MHz, but the tool had no reason to try, so that
number means very little either. It is measurable now for the first time.

### How it was found, which is the part worth keeping

Not by me, and not by inspection. An agent rebuilding the Field engine saw an
Fmax it did not believe and went looking for why. The warning had been printed
in every run for weeks; nobody read it.

That is the same lesson this project keeps relearning, and I have now written it
down enough times that it should be a rule: **the tool usually says what is
wrong, in plain text, and the failures come from not reading it.**

I reproduced the finding myself before accepting it, then fixed the script so
each block gets its own timing file naming the clock it actually has. Verified:
the tool now reports `10.000 clk` — the line that was missing every previous
time.

### Two side-effects worth knowing

**Real measurements are much slower.** One small block spent seven and a half
minutes just preparing placement, where the whole unconstrained measurement used
to take ten. Re-measuring everything is now hours, not minutes.

**And the machine cannot do three at once.** Running three measurements
concurrently exhausted it and two were killed. That is a scheduling constraint,
not a fault.

### Where the multiplier work stands

| block | before | after |
| --- | ---: | ---: |
| terrain detail | 28 | **3** |
| creature detail | 18 | **6** |
| Field IR engine | 79 | **4** (being re-measured properly) |

If the Field number survives the honest re-measurement, that is about **164 of
the 327 multipliers removed** — from a design that wanted three times what the
chip has.

---

## 2026-08-23 — I told you a block was broken. It wasn't. And the plan is now much better than mine.

### The correction first

I reported that sequencing the terrain detail-level block had **broken a fairness
guarantee** — the rule that one player's view degrading must never remove
geometry from the other player's. I said the composed test caught it, and I said
I had proved it by putting the old version back and watching the test pass.

**That proof was wrong, and the block was never broken.**

Putting the old file back forced a rebuild. The rebuild regenerated the
simulation model from clean source — and it *also* wiped mutant-corrupted files
that a testing tool had left lying around in test targets it never cleaned up.
So the test passing tracked **the rebuild**, not the file I changed. I changed
two things at once and blamed one of them.

Rebuilt properly, with every affected target wiped first, the sequenced version
passes all 72 checks and produces a trace **identical to the old block, number
for number**. The agent found this before I did, and it re-measured rather than
argued.

So: **28 multipliers down to 3 stands**, no repair was needed, and the block was
correct the whole time. What was actually broken was the test tooling, which is
now fixed and refuses to run unless it cleans every place the block is used.

I have committed the correction rather than quietly editing the record.

### The plan is no longer mine, and it is better

Your collaborator's brief is now the ruling. Three things in it change what I was
doing:

**A rule instead of improvisation.** I was fixing blocks one at a time by
instinct. The rule is: *give each subsystem the smallest local multiplier farm
its sustained rate actually needs, and share only what is mutually exclusive
inside that subsystem.* No console-wide sharing — terrain, texture, Field and
creatures genuinely run at the same time. And the sentence that reorders
everything: **multipliers are justified by what a frame actually demands, not by
preserving one-per-clock speeds that nobody ever measured a need for.**

**A number I said I could not invent.** You have now ruled it:

> **Zhaozhou v1 guarantees about 120,000 skinned creature vertices per frame at
> 60 Hz, and the Measure degrades before exceeding that.**

That turns the skinning block from "possibly justified" into a costed decision.
It currently spends 64% of the chip's multipliers chasing a speed its own
contract admits was never backed by a requirement. A rebuilt engine hits roughly
150,000 per frame — real headroom against your 120,000. And if that proves low
later, we build **two** of them; two small engines still cost far less than
today's single huge one.

**One thing I had backwards.** I had written off the terrain projector as
untouchable because its speed is genuinely spent. The better reading: it projects
the *corners of triangles*, so a patch does **6,144 projections for 1,089
actual points** — the same points over and over. Remember each point instead, and
a *slower* three-step projector finishes a patch in **half** the time while using
a third of the multipliers. Doing what I planned — making it slower without the
memory — would have been strictly worse.

### Targets

| part | multipliers now | target |
| --- | ---: | ---: |
| Field IR engine | 79 | 8–12 |
| creature skinning | 72 | 12–18 |
| terrain projector | 33 | 12–18 |
| texture unit | 28 | 8–12 |
| surface stamper | 28 | 4–6 |
| terrain normals | 18 | ~6 |
| creature culling | 15 | 4–6 |
| detail level (done) | **6** | was 18 |
| terrain detail (done) | **3** | was 28 |

If they land, the measured part of the design sits near **90–105 against 112** —
actual headroom instead of scraping the ceiling.

The Field engine is being rebuilt now, and it is the biggest single item on the
list.

### And a process change you should know about

Testing tools now run in their **own private copy of the project**. The mess
above happened because two things shared one build directory. That is no longer
allowed.

---

## 2026-08-23 — I have been calling the Field IR engine "done", and it had never been built

### What I found

The Field IR engine is the little instruction set that terrain, deformation and
formation programs are written in. I have reported it complete for days: every
operation has a test against the reference, every one has a score for how many
deliberate bugs the tests catch, and the sequencer that runs the programs is
verified with a mathematical proof.

All of that is true. **All of it is simulation.**

I checked how much of the design has ever been through the chip-building tool at
all. **46 of the 91 pieces have never been through it once** — and fifteen of
those are the whole Field IR engine.

So I put it through, as one unit, the way it actually gets used:

| | |
| --- | ---: |
| logic area | 10,623 cells |
| **hardware multipliers** | **79** |
| the chip has | **112** |

**One subsystem wants 71% of the chip's multipliers.** More than the terrain
projector and the surface stamper put together.

The running total is now **280 multipliers wanted against 112 available** — up
from 213 this morning — and half the design still has not been measured.

### Why this happened, and it is not a coding mistake

The engine contains ten calculating units — arithmetic, reciprocal, sine, square
root, length, normalise, curve, noise, ring, rotation. All ten are built side by
side, each with its own multipliers, permanently powered.

And the sequencer that drives them **runs one instruction at a time**, taking six
clock ticks each. So at any moment, nine of the ten are doing nothing while
holding their multipliers.

It is the same waste I described this morning, at the largest scale in the
design. The detail-level block had five idle multipliers and went from 18 to 6.
This has ten idle *units*.

**So the worst number in the design is also the biggest opportunity in it** —
worth more than the next three offenders combined.

### What I am not promising

Nobody has established that those ten units *can* share. The sequencer's
six-clocks-per-instruction pace and its mathematical anti-hang proof both assume
the current shape, and the proof would have to be redone against a shared design.
Every other block I have sequenced had an obvious answer; this one does not.

### What this means for "the Field IR engine is finished"

It is finished as a *design* and finished as *tested behaviour*. It is not
finished as *hardware*, and I should have been clearer about that distinction
before now. The same caveat applies to everything else in this project that I
have called complete: **nothing here has run on a physical board, and until
tonight, half of it had never even been asked whether it fits.**

### Meanwhile

The terrain detail-level block is being sequenced now by the same method, and
writing it a mutation sweep — which it did not have — immediately found a hole in
its tests.

---

## 2026-08-23 — the chip may not have enough multipliers, and nothing was watching

### The number

The chip has **112 hardware multipliers** (DSP blocks). I measured the blocks
built so far, one at a time:

| | |
| --- | ---: |
| multipliers the design asks for, **15 blocks measured** | **213** |
| multipliers the chip has | **112** |

**Nearly twice the chip, and most blocks have not been measured yet.**

### How much to worry

Less than that number looks, but not nothing.

Measuring blocks **one at a time gives each one its own multipliers**, and a real
chip shares them. So 213 is an over-count — a ceiling, not a prediction. The full
assembled chip currently reports **zero** multipliers used, because none of these
particular blocks is wired into it yet.

But the blocks driving the total are not alternatives to each other. Terrain
projection, terrain detail-level, surface normals, the texture unit and the
triangle binner are all working in any frame that draws ground. They will be
present together.

### The good news: it is mostly waste, not arithmetic

Every one of these blocks computes several multiplications **at the same time**
when it does not need to. The detail-level block runs **once per creature per
frame** — a few thousand times a second at most — yet it does five multiplications
simultaneously, as if it ran every clock tick.

I already proved the fix works on that block by accident: it went from **28
multipliers to 18** just by not asking for wider arithmetic than it needed and by
noticing it computed the same product twice. An agent is now doing the real
version — feeding those five through **one** multiplier over a few clock ticks —
with instructions to revert and tell you if it does not pay off.

If that works, the same lever applies to the four biggest offenders, which are
28-33 multipliers each.

### The part that is a process failure, not an engineering one

**Nothing was watching this number.** The ledger — the file that is supposed to be
the schematic — has fields for multiplier budgets and measured multiplier counts.

- Blocks declaring a multiplier budget: **zero**
- Blocks recording a measured multiplier count: **zero**
- Rules checking either: **zero**

The equivalent check for *logic area* has existed all along, which is why area has
never surprised us. Multipliers had no such check, so the design walked to twice
the chip's capacity with every gate green. The numbers were measured and written
into a report — and never carried back into the ledger.

I have not added the rule yet, on purpose: whether it should be a budget, a
running total, or a hard stop depends on whether the pilot shows the 213 is waste
or real work. Building the rule first would mean guessing.

### Where this leaves "finished"

There are now **three** gates in front of the design fitting the chip, not one:

1. **Timing** — currently 20% too slow, and the measured blocker is one crossing
   sitting on your desk.
2. **Multipliers** — 1.9x over, newly discovered, pilot running.
3. **Twenty-three blocks still unbuilt**, eleven of which are waiting on your
   decisions about game behaviour rather than on any work.

---

## 2026-08-22 (night) — the clock seam is fixed, and a test score I published was wrong

### The seam between the two clocks is gone

The graphics chip and the video output run on two different clocks. There was a
piece of checking hardware straddling them: it sat on the graphics clock but
reached across to grab **every single pixel as it went out to the screen** —
122,880 grabs per frame — with nothing but a phase relationship holding it
together.

That worked in simulation for a reason that should worry anyone: the simulation
runs the two clocks in perfect lockstep. A real board does not. This is the
failure class that cannot be fixed by running slower — the data arrives at the
wrong *time*, not too late, and there is no clock speed that repairs it.

It now sits entirely on the video clock, and only one 32-bit number crosses,
once per frame, with a proper handshake. **The picture is unchanged, byte for
byte** — which is how we know it still does the same job.

It also settled a disagreement between three documents: the contract said video,
the ledger said graphics, and the hardware believed the ledger. They now agree.

### A number I gave you was wrong, and here is the correction

I have been reporting mutation-test scores — deliberately breaking the hardware
in specific ways and checking the tests notice. For the detail-level block I told
you **21 of 22 caught**.

**The real figure is 22 of 23**, and the difference is not rounding. Three of
those "caught" breakages **never compiled at all.** The test binary lives outside
the folder that gets wiped between attempts, so when a broken version failed to
build, the *previous* binary was still sitting there and ran instead. A build
failure was being recorded as a successfully-caught bug — the most flattering
possible way to be wrong.

I did not find this. The agent working on the clock seam found it in its own
work, and I checked mine against it. Two of my three were a typo in how I wrote
the breakage; one was a comparison the compiler rejects.

The fix is not just to notice sooner. Every deliberate breakage is now **compiled
before any of them is scored**, and the run refuses to start if one of them
doesn't build. A broken test is now a refusal rather than a quiet inflation.

I have corrected the figures everywhere I published them rather than leaving them
standing.

### Runs — you were right, and it went back further than this session

You pointed out I had stopped creating run records. It was worse than that: the
script that creates them lives one folder *above* the project, and **that folder
is not under version control at all.** Sixteen run records were sitting there
unprotected, going back to the first day.

All sixteen are now inside the project where they are backed up. Three days of
work had no record at all and have been reconstructed from the commit history —
marked clearly as reconstructions, because what a reconstruction *cannot* recover
is exactly the valuable part: what you asked for in your own words, what I tried
and abandoned, and what the agents found.

The process is now written down inside the project, so it does not depend on
anyone remembering that the tooling is somewhere else.

### What is running right now

- A full chip fit, to measure what the clock-seam change did to the speed. This
  is the measurement you ruled had to come before any more speed tuning.
- An agent building the object-culling hardware — the part that throws away
  creatures the camera cannot see before spending any work on them.

### Still waiting on you

1. **The two clocks are still told to relate to each other** in the timing rules.
   Cutting that relationship would improve the numbers, but only by telling the
   tool to stop looking. That is your call, not mine.
2. **One last crossing remains** — a counter read across the same seam. If the
   two problems we measured were on *that* rather than the one I fixed, the
   re-measure will still show them.
3. The three earth-field write operations still have no defined behaviour, and
   the terrain patch block sits behind them.
4. The meshlet descriptor format — where an object's bounding sphere lives in
   memory.

---

## 2026-08-22 (late) — the speed change paid off, and your four rulings are in

### The measurement you were owed

| | before | now |
| --- | ---: | ---: |
| worst path | 10.729 ns | **10.475 ns** |
| endpoints too slow | 97 | **56** |
| logic cells | 7,667 | **7,415** |
| hold problems | 0 | **0** |

Faster, 42% fewer slow endpoints, and smaller again. That came from the five
copies of one counter rule becoming a single proven piece — the change I could
not test and had to prove instead.

The machine currently runs at about **95.5 MHz**. Your 120 MHz target is on the
docket with an honest costing: it is 20% further, not 5%, and the easy 55
nanoseconds are already gone.

### Your four rulings, all acted on

1. **One engine, five profiles.** Done. The five FIELD.SEQ entries were each
   demanding their own reference model, their own two test files, and their own
   share of the chip — **five engines worth of area booked for one engine.**
   They are now marked as configurations of the one real sequencer, and I added
   a rule (with six tests) so nobody can quietly turn a configuration back into
   a phantom block.
2. **"Visibility sectors" deleted; bounding-sphere frustum culling instead.**
   Recorded as the law, including your point that GEOM.CLIP is far too late in
   the pipeline to save the work. I have not built it yet — the LOD third of
   that block is what I built today.
3. **The LOD overflow: fix the law, never bake the wrap into silicon.** Done,
   and your amendment was the important half — widening it to 64-bit would
   NOT have been enough, because the next line multiplies by 9 or 11 and
   overflows again. The boundary is now never computed at all.
4. **BALANCED stays; fix the CDC seam first.** Agreed and followed. I have
   stopped chasing the remaining half-nanosecond.

### The new piece: creature LOD, which is your top-priority area

This is the part that decides how much detail a creature gets as it moves away
from the camera — full mesh, reduced mesh, blob, speck — and it has to be
*stable*: a creature that flickers between two levels of detail looks broken
even though both frames are individually correct. So the rule holds a choice
for a minimum time and only switches once you are 10% past the boundary.

**It needed division, and division is expensive in hardware.** Two of them per
creature per frame. I removed both — not by approximating, but because every
one of those divisions was feeding a comparison, and "is A/B bigger than C" can
always be rewritten as "is A bigger than C times B". Exact, and no divider.

### Two bugs found, and one of them was mine

**In the shipped reference:** the boundary calculation overflowed and went
NEGATIVE for perfectly ordinary creatures — a well-decimated model with a
generous error budget. A negative boundary means the detail level can never get
coarser, so a creature walking into the distance stays at full detail forever.
That is a frame-rate bug that would have been very hard to find by looking.

**In my own new hardware:** I had written the three detail levels as a loop,
and the loop silently gave all three the same error value — so the block could
only ever answer "full detail" or "speck", never the two middle levels. It
agreed with the reference on **27,618 of 29,459 checks** anyway, because most
situations legitimately land on one of the two extremes.

It was caught by the deliberately absurd test cases, not the realistic ones.
There are exactly three detail levels and there always will be, so the loop was
saving nothing and hiding something. It is three plain lines now.

---
## 2026-08-22 — a subtraction that was never needed, and a law nothing was checking

Two things came out of the same line of code today, and the second is the one
worth your attention.

### The small one: a wide subtraction that was always a free operation

The slowest remaining piece of the design was a counter in the controller-input
block. It was written to ask "is there room left before this counter hits its
maximum?" like this:

> take the biggest number the counter can hold, subtract the current value, and
> compare

Subtracting from *all ones* is the same thing as flipping every bit — which
hardware does for free, instantly. The subtraction was doing real work to
produce an answer that was already sitting there. The same pattern appeared in
**five places across four blocks**, and they had each been written out by hand,
not even in the same style.

They now all call one shared piece. Whether that actually moves the headline
speed number is being measured; the design is at the point where the limit is
mostly *distance across the chip*, not logic, so I will report the measurement
rather than the reasoning.

### The one that matters: the tests could not tell if it were wrong

Before changing five places at once, I broke the rule deliberately — removed the
"is there room?" check entirely, so the counter would wrap around to zero
instead of stopping at its maximum — and ran everything:

> **48 directed checks passed. 5,000 random packets, 19,015 checks, passed.**

The broken version sails through the whole test suite.

That is not a gap I could close by writing more tests. These counters are 64
bits wide and go up by a few at a time, so reaching their maximum would take
roughly **18 quintillion events**, and nothing outside can set them near the
edge to check what happens. The one situation the safety check exists for is a
situation no simulation can ever produce.

So the evidence changed shape. Instead of another test, the rule is now handed
to a **prover**: a tool that checks the law for *every possible pair of numbers
at once*, mathematically, rather than for the examples someone thought to write
down. It confirms the counters stop at the top instead of wrapping, that adding
zero changes nothing, and that once stopped they stay stopped.

Then I attacked the proof to make sure it was not just agreeing with me: seven
deliberate breakages, **six caught**, including the exact one the entire test
suite missed. The seventh turned out to be genuinely the same behaviour written
a different way — recorded as such, not left looking like a hole.

**Why this matters beyond one counter.** A counter that silently wraps reports a
*lower* error rate than the truth. It is the failure that makes every other
number you read untrustworthy, and it was sitting in five places with nothing
checking it. The proof now runs in the fast lane, every time, in under three
seconds.

### Also worth saying: my own check caught me

The repository has a rule that you may not state something is guaranteed
without naming what guarantees it. I wrote a comment claiming this was "right by
construction" — and the check refused the commit. It was right to. The fix was
not to soften the sentence; it was to go build the thing that makes it true.

---

## 2026-08-22 (end of day) — the speed campaign, and where it stopped

| | at the start | now | target |
| --- | ---: | ---: | ---: |
| worst path | 65 ns | **10.7 ns** | 10 ns |
| slow paths | 13,651 | **245** | 0 |
| endpoints too slow | 3,746 | **97** | 0 |
| logic cells | 9,181 | **7,667** | 41,910 |

**From 550% too slow to 7% too slow, with 98% of the slow paths gone, and the
design about 1,500 logic cells SMALLER than when the day started.**

### Seven fixes, and every one made it faster AND smaller

The problems were all the same shape: arithmetic doing too much between two
clock ticks. The checksum computing one bit at a time. A length recalculated
every tick and sent four places at once. A validity check reading a memory,
looking up a table and deciding, all in one tick. A counter whose enable was an
entire address range-check fanning out to 32 flip-flops.

None of them was a sign the design is too slow. They were accidents.

### Where I stopped, and why

No remaining problem is bigger than 0.75 ns, and they are spread across five
different parts. That is fine-tuning, and each attempt costs a 25-minute
compile to judge.

More importantly: **the design is now placement-bound.** I proved the compiler
is deterministic — identical input gives identical output — and then watched
three well-reasoned changes each do exactly what they were designed to do while
making the overall number worse. At this margin the compiler's layout decisions
matter more than the logic, so further small changes are as likely to lose as
win. Two of those three I reverted; one I kept, and the reasons are written down.

The next useful moves are a different kind of work — the two decisions on your
docket — not more of this.

### What is waiting on you

1. **Fitter effort.** Measured both ways: 97 slow endpoints with clean hold
   timing, or 17 with two hold faults. Neither is finished. One line either way.
2. **The GPU/video crossing.** Four hold faults keep appearing and disappearing
   there across runs regardless of what I change — three separate compiles now.
   They are not fixed, they are lucky. The documents disagree about which clock
   that lane belongs to, and the code picked one.
3. **The rasteriser, particles, compositor and 2D blocks** — game behaviour I
   have deliberately not invented.

Still simulation and compiler output only. No hardware has run any of this.

---

## 2026-08-22 (late) — speed: from 550% too slow to 6% too slow

| | at the start | now | target |
| --- | ---: | ---: | ---: |
| worst path | 65 ns | **10.64 ns** | 10 ns |
| paths too slow | 13,651 | **1,995** | 0 |
| logic cells | 9,181 | **7,648** | 41,910 |

**Six percent over, from 550% over.** And the design uses about 1,530 fewer
logic cells than when this started — every single fix made it both faster and
smaller.

### Six fixes, all the same kind of problem

Every one was a piece of arithmetic doing too much between two clock ticks.
None was a sign that the design is fundamentally too slow.

The last one is the one I would want you to know about, because it is the
mistake rather than the fix. I had **proved** that a particular length can only
ever be one of three values — 0, 16 or 28 — and used that proof to simplify
something else, and wrote the proof down in a comment. Then the code went on
computing that value the long way anyway: an addition, a comparison, a
subtraction and two more comparisons, every time.

**Eighteen thousand of the twenty thousand remaining slow paths ran through
that gap.** Replacing it with two equality checks took the worst case from
1.32 ns over to 0.64 ns over.

A law you have proven is worth nothing until the logic is written in its terms.
The law is now also machine-checked, so it cannot quietly stop being true.

### What is left

Two thirds of the remaining slow paths are a single video-to-guard connection
sitting 0.195 ns over — close enough to be noise. The rest are small and
shallow.

Timing is **not** closed. Closed means zero paths too slow, and there are 125
endpoints still failing. But there is no longer a big structural problem
visible; it is fine-tuning from here.

Still simulation and compiler output only. No hardware has run any of this.

---

## 2026-08-22 (evening) — speed: 6.5x too slow, then 1.29x, now 1.11x

Three fixes, each measured on the real chip compiler.

| | at the start | now | target |
| --- | ---: | ---: | ---: |
| worst path | 65 ns | **10.9 ns** | 10 ns |
| paths too slow | 3,746 | **172** | 0 |
| logic cells | 9,181 | **7,713** | 41,910 |

**The console is now 11% too slow instead of 550% too slow, and it uses about
1,470 fewer logic cells than when the work started.** Every fix made it both
faster and smaller, because the slow structures were also the bulky ones.

### What the three fixes were

1. The packet checksum computed one bit at a time, 64 steps deep per chunk.
   Replaced with a wide version about seven deep — provably identical
   arithmetic, checked on every single-bit input and 200,000 random cases.
2. The same checksum in the debug blitter.
3. A length calculation in the blitter that was redone from scratch every
   clock tick and sent to four places at once. It only changes once per chunk,
   so it is now remembered instead of recomputed.

### What is left

No single big thing. The remaining 172 slow paths are spread thin, and the
worst of them is the packet header check: one clock tick that verifies the
magic number, the version, the reserved flags, four length rules, the
checksum, the frame epoch, and then sets up the next stage. That is too much
for one tick and wants splitting in two.

That is ordinary sequencing work. Nothing found so far suggests the design is
inherently too slow — all three problems were accidental depth, not
architecture.

**One honest gap:** an audio counter path that measured badly early on has not
appeared in any report since, but it has never been confirmed fixed. The
reports only keep the worst 100 paths, so its absence is not proof. That needs
a full listing rather than an assumption.

Still simulation and compiler output only. No hardware has run any of this.

---

## 2026-08-22 — the speed problem is 95% solved, and the design got SMALLER

Last entry said the console fit the chip but ran **6.5x too slow**, and that
this was now the biggest open problem. Here is where it stands after the fix.

| | before | after | target |
| --- | ---: | ---: | ---: |
| worst path | 65 ns | **13 ns** | 10 ns |
| paths too slow | 3,746 | **584** | 0 |
| logic cells used | 9,181 | **7,633** | 41,910 |

**From 6.5x too slow to 1.29x too slow.** And it uses ~1,550 FEWER logic cells
than before, because the slow thing was also the bulky thing.

### What was wrong, in one sentence

The checksum the console uses to verify every packet was computing **one bit at
a time**, sixty-four steps deep for a single chunk of data — so the chip had to
do sixty-four things in a row between two clock ticks. It now does the same
arithmetic in one wide step about seven deep, which is the standard way to
build these and is provably identical to what it replaced.

### It is provably the same arithmetic

That last point matters more than the speed. A faster checksum that computes a
different number than the rest of the machine would be worse than a slow one.
So the new one is checked against the shipped checksum on every possible
single-bit input, on 200,000 random cases, and against twenty deliberate
sabotages of its own logic — all caught.

The command block's packet handling is unchanged: same tests, same results,
same failure ordering, and the mathematical safety proof still passes.

### Three side effects worth knowing

* **The video/audio timing faults disappeared.** Three had appeared in the
  previous run; with the checksum pressure gone the compiler no longer has to
  contort around them. That is luck rather than a fix, and the underlying seam
  still needs a real decision — see the docket.
* **A guard now exists** so the slow checksum cannot quietly come back. It
  would pass every functional test if it did; only a chip compile would notice.
* **The remaining 13 ns path is ordinary work.** It is one register in the
  debug blitter feeding four destinations through a subtract and an address
  calculation. That is a pipelining job, not a redesign.

Still simulation and compiler output only. No hardware has run any of this.

---

## 2026-08-22 — THE WHOLE-CHIP TEST RAN. It fits. It is far too slow.

The full-console compile completed for the first time. Two answers, and they
point in opposite directions.

### It fits, with room to spare

| | the design | the chip |
| --- | ---: | ---: |
| logic cells (ALMs) | **9,167** | 41,910 — **22%** |
| registers | 9,571 | |
| memory blocks | 13 | 553 |
| multipliers (DSPs) | **0** | 112 |

Under a quarter of the chip, and not one multiplier used yet. That is a lot of
headroom for the rasteriser and creature work still to come.

### It does not run fast enough. Not close.

| | target | worst measured |
| --- | ---: | ---: |
| GPU clock | 10 ns (100 MHz) | **65 ns** |

It misses by about **6.5x**, on **3,595** separate paths. The video and audio
clocks both pass comfortably; the whole problem is in the GPU domain.

**This is now the single biggest open problem in the hardware.** Fitting was
the question everyone was worried about, and it turned out fine. Speed is the
one that actually bites.

To be clear about the flavour of the failure: nothing is broken and nothing is
wrong. The design does what it should; some paths just do too much work between
two clock ticks and need to be split across more ticks. That is ordinary
hardware work, but there is a real amount of it.

### What I will NOT tell you yet

I have a strong suspicion about which part is responsible — the command
block's checksum, which currently chews through 32 bytes in a single tick, and
that is exactly the shape that made the same block uncompilable last week.

I am not reporting that as the cause, because I have been wrong about this
specific block four times in a row this week, every time by reasoning about the
code instead of reading the compiler's report. The next step is to ask the tool
which paths are slow and then say so.

### One more measurement worth having

The compile is also much cheaper than the record claimed. It was written down
as taking 42 minutes and 6.2 GB, and was the reason a second, larger machine
was once briefed for this job. Measured today: **4 minutes and 5 GB** for the
same stages. A large part of that was the command-block memory defect fixed
earlier today.

Still simulation and compiler output only. No hardware has run any of this.

---

## 2026-08-22 (later still) — the debug blitter is done, and the whole-chip
## test is finally unblocked

### DEBUG.FRAMEBLIT is finished and it is tiny

This is the block that copies a debug picture from the computer's memory into
the console's framebuffer. It was split out of the command block months ago
precisely so a debug-only feature could never be the reason the console does
not fit. It now measures **962 logic cells of 41,910 — 2.3% of the chip**, with
no memory blocks and no multipliers.

It has moved up a rung to RTL_VERIFIED.

### What I actually tested, because "it copies pictures correctly" is not the
### hard part

This block's real job is a promise: **a half-written picture must never become
visible.** If a transfer fails, gets interrupted, or arrives corrupted, the
screen must keep showing the old frame rather than a torn one.

The trouble is that a test which only checks "good picture in, good picture
out" would pass no matter how badly that promise is broken, because a
successful transfer looks the same either way.

So I broke the promise twenty different ways and checked the tests noticed:
publishing before the writes finish, publishing before the checksum is checked,
publishing when the frame slot has already been given to someone else,
releasing a slot while writes are still in flight, releasing a slot the block
never owned, folding abandoned data into the checksum, ignoring a memory
error. **All twenty were caught.**

### The whole-chip test is unblocked for the first time

Until today the record said the full-console compile was blocked by one thing:
a memory inside the command block that the chip compiler could not turn into
real memory. That is the exact defect fixed in the previous entry.

**That blocker is gone**, so I am starting the full compile now. It takes hours
and I will report what it says — including if it fails, which is itself a
useful answer.

To be clear about what that test is and is not: it asks whether the design
fits the chip and runs fast enough **inside the compiler**. It is not a
programmed board and not fabricated silicon. Nothing has run yet.

### Two measuring instruments were broken, and both had been lying quietly

Worth recording because both are the same shape — a check that looked like a
guard while answering the same way regardless of the truth.

1. The test runner reported **every one of 337 tests as failing**. Nothing was
   wrong with the tests. The computer has two copies of the build tool
   installed, and the wrong one was being picked up; it misreads Windows paths.
   One wrong conclusion away from a day spent debugging nothing.

2. Every chip-compile result ever recorded — **all 42 of them** — carried a
   flag saying "the source code was not in a clean state when this was
   measured", which would make every number unattributable. The flag had
   **never once been true**, because it was asking a second copy of `git` that
   disagrees about Windows line endings. It reads correctly now, and the 43rd
   row is the first honest one.

Neither broke anything that shipped. Both would have made a real problem
invisible, which is worse.

---

## 2026-08-22 (later) — the command block now fits, with room to spare

Last section said `CMD.DMA` compiles but does not **fit** — it needed more of
the chip than the chip has. That is fixed, and the size of the fix is worth
stating plainly.

|                        | before  | after      | the chip has |
| ---------------------- | ------- | ---------- | ------------ |
| logic cells (ALMs)     | 83,977  | **3,607**  | 41,910       |
| registers              | 33,680  | **1,571**  |              |
| dedicated memory bits  | 0       | **32,768** |              |

From **twice the whole chip** to **8.6% of it**. The block went from the single
worst thing in the design to a rounding error.

Those are *fitted* numbers, not estimates — the chip compiler placed and routed
it, which it had never once managed for this block before. It uses 4 of the
chip's 553 memory blocks and none of its 112 multipliers.

Still simulation and synthesis only. No hardware has run any of this.

### What was actually wrong

I had four theories about this block and every one of them was wrong. So I
stopped theorising and asked the compiler what it had built.

It had built the 4KB packet staging buffer out of **32,768 individual
registers** — a filing cabinet made of 32,768 separate drawers, each with its
own lock, when the chip has purpose-built memory blocks sitting unused. The
giveaway was one line in the report: `Total block memory bits: 0`. Not "a bit
too much memory used" — **none at all**.

That also explains why my three attempts to simplify the wiring each moved the
number by about 0.02%. I was rearranging the handles on the drawers.

### Why it took four wrong guesses

The chip compiler takes about 45 minutes per attempt on this block, so each
theory was expensive, and I kept spending that time on *reasoning about the
source code* instead of *reading the report the compiler had already written*.
The report was there after the first run. I did not open it until the fifth.

The lesson is cheap to state and I would rather have it written down than
learned again: when a measurement is available, measure. Four inferences cost
more than one look.

### One real bug found on the way, which simulation could never have caught

Setting up a clean baseline turned up a **failing proof** on this same block —
one of the mathematical checks, as opposed to the run-it-and-see tests.

It was mine. I had earlier bounded a loop at 64 steps and written that the
bound was safe "because the memory bridge only ever delivers 64 bytes here."
The proof disagreed: nothing in the hardware *makes* the bridge stop at 64. It
passes the "this is the last piece" signal straight through from the outside
world without counting. A bridge that misbehaved would overrun the buffer.

The consequence was contained — the packet would fail its checksum and be
rejected — so this was a false statement rather than a hole. It is still a
false statement, and I had written it down as a proof.

Fixed so the hardware no longer takes anyone's word for it: each transfer now
records where it ends and stops there.

**Then I checked which test would have caught it.** Answer: none of them.

| test                          | result   |
| ----------------------------- | -------- |
| directed tests                | passed   |
| random, 400 packets           | passed   |
| random, 5,000 packets         | passed   |
| the mathematical proof        | **caught it** |

The simulated bridge is well-behaved by construction, so no amount of random
testing could ever produce the misbehaving one. That is exactly what the proofs
are for, and it is the first time this project has had one earn its keep on a
bug I introduced.

### And one test that was checking nothing

While reworking the buffer I found the 5,000-packet random test **never
checked the contents of the packets it streamed**. It verified the verdict,
the byte count, and that broken packets produced nothing — but for good
packets it never compared a single byte against what was sent.

Now it does. To prove the new check actually works I deliberately broke the
buffer logic: it now fails 60 of 1,542 checks. Before, it would have passed.

---

## 2026-08-22 — the spell engine runs thirteen of sixteen instructions

Two results, and one honest note about how often I was wrong on the way.

### The Field IR engine can now run programs

The thing that executes your terrain, weather and spell programs understood
**three** instructions when this session started. It understands **thirteen**
now. Everything except the three curve-table instructions, which need a piece
of plumbing the others do not.

That matters because the instructions were all built and tested individually
weeks ago. What was missing was the part that actually *runs* them one after
another — handing operands to a unit, waiting however many clocks it needs,
and putting the answers back in the right registers. Several instructions
produce **three** answers at once and there is only one place to put one at a
time, so that had to be walked.

Every instruction is checked against the software version of itself on the same
program: **613 fixed tests and about 3,000 random ones**, all matching exactly.

### The command block can be built at last

`CMD.DMA` had never once made it through the chip compiler. Not "fitted badly"
— never completed at all, in any form, ever. It does now.

The cause turned out to be one number. A loop was written to run 192 times
where **at most 28 iterations can ever happen**, and the compiler was building
a chain of roughly 1,248 dependent stages in order to throw most of it away.
Changing 192 to 64 fixed it, and I could prove the removed iterations were
unreachable rather than merely unlikely.

It still does not **fit** — the block needs more of the chip than the chip has,
by itself. But that is now one precisely identified array with a known remedy,
rather than a block nobody could compile.

### Where I was wrong, three times

Worth telling you plainly, because it is a pattern rather than an accident.

Three times I wrote a conclusion into the repo and a measurement then
contradicted it:

- I recorded that the loop above needed a substantial redesign. **It needed a
  bound check.**
- I predicted a change would shrink that block. **It grew it by 14,000 units**,
  and I reverted a change that was provably identical in behaviour — because
  identical is not the same as better.
- I predicted a test mutation would hang the machine. **It passed cleanly.**

And the test sweep found something worse than any of those: I had written a
safety check for the wide instructions and never written the same check for the
narrow ones, so two of them could quietly overwrite a register they do not own
and nothing would have noticed. That is a hole in a whole class, not a missing
case.

**I am reliably right about where a problem is and unreliable about what fixes
it until something measures it.** That is an argument for the way you asked me
to work, not against it — every one of those was caught by the rule that says
measure before believing.

Everything above is simulation. Nothing has run on a board.

---

## 2026-08-21 (evening) — the blitter is in, and the DSP number was never real

Three things. The first two are finished; the third is why the DSP work was
starting from a false premise.

### Your decision is in, and it cost nothing

You said take the speedup. It is taken, it is on `main`, and the whole suite is
green.

**The picture never changed.** All 41 failing checks were timing assertions, so
I re-derived each law rather than editing numbers until the tests went quiet.
The Duo capture moved by **five bytes**: one counter — the machine recording
one fewer missed deadline — and the four-byte checksum of the section holding
it. I proved that arithmetically before accepting it. The Z60 and Storm
captures are **byte-identical**. Not a single frame checksum moved.

Something I nearly got wrong. The phase shift happens **only in Duo**. My first
attempt applied the Duo law to all three modes and broke two of them, and the
reason I caught it is that the failures contradicted what the other test had
shown — so I went and measured all three instead of assuming. The three modes
have different frame lengths, and the ~58,000 cycles you gained only crosses a
frame boundary in Duo. That is written into the test now, marked as a
measurement rather than a rule, so whoever changes the timing next knows what
to re-check.

### The big buffer is gone

`CMD.DMA` used to hold a **whole screen** of pixels on the chip while it worked
— 1.97 megabits, roughly a third of all the memory the chip has. The new
blitter streams instead, and holds **64 bytes**.

That buffer was also the thing stopping the full-chip fit from completing.
Quartus could not turn it into real memory, so it kept failing with a specific
error, and elaborating that one block alone once needed **16.2 GB**. It does not
need fixing now. It is not there.

### The DSP number you have been told is not the console's

You sent me an audit saying the "171 DSPs" figure rests on a stale measurement.
It does, and it is worse than the audit thought.

**That census measures 35 of the 88 blocks in the repository.** It is 93 commits
old, it says of itself that the code was not clean when it ran, and seven of the
rows it does list produced no data at all — five timed out on a limit that was
later found to be too short, and two failed outright.

The 46 blocks never measured include the biggest multiplier users written since:
the geometry projector, the creature skinner, the pose maths, and the whole
Field IR engine.

So **171 is not too high or too low. It is a number about a different design**,
and my own arithmetic subtracting from it was meaningless. Nothing gets planned
from it again.

I had three agents check every claim in your audit against the actual code
rather than acting on paper. The audit holds, and four things changed:

- **`TERRAIN.LOD` is a better target than the audit thought.** It does one
  decision per patch, not per subpatch — sixteen times less work than assumed,
  with about eleven times more headroom than it needs. It is holding 24
  multipliers to do it.
- **The Field IR engine is much worse than the audit thought.** One block alone
  holds **ten** multipliers running in parallel behind a 34-cycle wait. The
  engine totals 28, not the 13 the audit assumed — and that is per engine, with
  five of them planned.
- Both projectors are 11 multipliers, not 9.
- The texture unit has **twelve duplicate multipliers**: four copies of the same
  block computing the identical four numbers from the identical inputs. Sharing
  them costs nothing at all — no maths changes, no speed changes, and unlike
  the audit's own suggestion it does not throw away an existing proof. The
  contract had already written down that this was the fix if the measurement
  ever showed the sharing had not happened. It shows exactly that.

**And a contract is wrong about its own block.** `TERRAIN.LOD.md` says four
comparators and no multipliers; the block has twelve and twenty-four. Same
shape as the `abs` bug from this morning — a document and a design disagreeing
with nothing checking.

### What I am doing now

Re-measuring every block at the current code, starting with the one that
failed outright before, because it is the direct test of whether removing that
buffer worked.

Then three changes that are exact and cost nothing, in order of certainty. I
checked two of them myself rather than trusting the reports: the blend rewrite
is identical across all 196,352 possible inputs, and the skinning rewrite is
identical across its whole legal range and wrong outside it — so that one gets
a guard naming who guarantees the range, not a comment claiming it.

Everything above is simulation. Nothing has run on a board.

---

## 2026-08-21 (later) — the sequencer is built, and CI was red for a reason

Two things landed since the note below.

### The Field IR sequencer runs programs now

The last piece. `FIELD.SEQ.CORE` is the register file and the walk that turns a
compiled program into a run: zero the file, load the inputs, execute until the
program says stop, read the outputs back. Sixty-four registers, six clocks per
instruction.

It runs the arithmetic instructions today — fifteen of them. The multi-clock
ones (reciprocal, sine, length, normalise, the curve tables, noise, rotation,
ring) are all built and verified but not yet plugged into the walk, so the five
`FIELD.SEQ.*` profile blocks stay unbuilt until they are. That is the next
piece of work and it needs nothing from you.

An instruction the walk does not recognise is **refused**, and the run stops
with a status. It is not skipped and it does not return zero, because a
sequencer that quietly ignores an instruction produces a field that looks
plausible and a world that is wrong.

**102 directed checks plus about 1,850 random ones. I broke it nineteen ways on
purpose; seventeen were caught, and the two that were not are a recorded pair
that cannot both be removed.**

### It found a bug in something I had already signed off

The arithmetic unit's `abs` was wrong for exactly one input — the most negative
number there is. The written law says it should saturate. It did not.

The reason nothing caught it for weeks is the interesting part. The arithmetic
unit's own test did not ask the real reference. It **restated** the rule in its
own words, restated it the same wrong way, and then asserted the wrong answer —
under a comment I had written warning that this exact thing could happen. So the
hardware and its test agreed with each other and the actual law was outvoted.

The sequencer's test does not restate anything. It runs whole programs through
the shipped reference itself, which is why it saw immediately what nothing else
had. The arithmetic unit is fixed, and its test now asks the real thing too.

**A wrong restatement agrees with a wrong implementation forever.** That is the
lesson and I have applied it where I can.

### You were right about CI

Every push has been failing, and it was one thing: code formatting.

The check that catches it needs a specific tool. That tool is not installed on
this machine, so locally the check reported "skipped" and the suite went green.
On the server it ran and went red. **Both answers were accurate and neither was
useful**, which is the worst possible shape for a test to have.

Thirty-five files had drifted, some of them from long before this week. All
reformatted. The real fix is that the tool is now pinned as a project dependency
at the exact version the server uses, so `npm install` puts it on any machine
that builds this repo and the local check stops skipping. It runs here now.

### Where the hardware stands

**37 specified · 4 reference-complete · 37 unit-verified · 14 rtl-verified**
across **92** blocks.

Everything above is simulation. Nothing here has run on a board.

---

## 2026-08-21 — the new blitter works, is faster, and I need one decision from you

Step 4 is built: the new frame blitter and the slot manager are wired into the
real machine, the blit dispatch has moved off the command path, and every piece
your review asked for is connected — the guard window is now the lease, slot
readiness comes from a publication that was actually accepted, and the write
queue can finally say "not yet" instead of reporting afterwards that it lost
pixels.

**It works. Every blit succeeds and every frame is the right picture.**

It is on a branch called `blit-integration-step4`, not on the main line, and
`reports/BLIT_INTEGRATION_PHASE_SHIFT.md` explains why in full. The short
version is below.

### It is faster than the thing it replaces

|  | first blit finishes | period |
| --- | ---: | ---: |
| old | 605,308 | 637,184 |
| new | **547,321** | 637,184 |

**About 58,000 cycles faster.** I expected it to be slower and spent a while
looking for what I had broken before measuring properly.

The reason is the redesign itself. The old blitter fetched an entire screen from
host memory and *then* wrote it to video memory, one after the other, because
the old rule forbade writing anything before the checksum was verified. The new
one does both at once. Your demo's own notes list "streaming blit" as one of the
changes that would be needed to reach 60 Hz. This is that change.

### And that is the problem

The picture is identical. The frame rate is identical. But every frame now
arrives **one frame earlier**, and the Duo demo checks its timing against a
blitter that took 338,000 cycles. So it fails 41 of 340 checks — not one of them
a wrong pixel. What fails is the half-rate pattern, the missed-deadline count,
and a per-frame deadline marker.

`reports/status/phase2_wave2.md` explains at length why 60 Hz is infeasible,
built on that 338,000 figure. **The figure is now about 58,000 lower.** Not
enough to reach 60 Hz — but it is no longer the machine's number.

### Why I did not just update the reference file

Because it is not only a picture. It encodes a **measured property of the
machine** that the demo asserts as a law in code and that a report explains in
prose. Quietly regenerating it would re-baseline that law, and the next person
to read the report would find numbers that describe nothing.

The picture being identical is exactly what makes it tempting to just regenerate
and exactly why I should not.

### What I need from you

1. **Take it** — I update the reference, the demo's expectations and the
   infeasibility report to the new measured cost. The machine gets faster and
   the record follows.
2. **Keep the old timing** — I deliberately hold the blit back so it lands in
   the same frame as before, throwing the improvement away on purpose.
3. **Something else**, if that timing matters for a reason I have not seen.

**I would take option 1.** The improvement is real, it is the direction your own
review pushed, and the report is out of date either way.

### One thing worth knowing, found only by wiring it

The blitter reads its permit number on the *same clock edge* it accepts a job.
So the permit has to be granted **before** the job arrives. Asking for the permit
and handing over the job on the same edge makes the blitter memorise a number
from before it was issued — and then every finished frame is rejected as stale by
a slot manager that is working perfectly. Nothing in any document said this; it
only appears when the two are actually connected.

### Where the hardware stands

**37 specified · 4 reference-complete · 36 unit-verified · 14 rtl-verified**
across **91** blocks. Main is green.

---

## 2026-08-21 — every Field IR operation is built

The instruction set the terrain, weather, particle and deformation programs are
written in is **complete in hardware**. `NOISE2`, `RIDGE`, `ROT2`, `ROT3` and
`RING` landed today; the earlier pieces were already in.

`reports/FIELD_IR_ENGINE.md` has the table — every op, its test counts and its
mutation score.

What is left is the **sequencer**: the thing that reads a compiled program and
runs it, one instruction at a time, over the pieces now sitting there. That is
the last thing standing between the five `FIELD.SEQ.*` blocks and being real,
and it needs nothing from you.

### Two things worth telling you

**My mutation harness was lying to me, and I caught it.**

The way each piece gets finished is that I break the hardware eighteen different
ways on purpose and check the test notices. For `RING` it reported seventeen of
eighteen caught. That was wrong.

One of the deliberate breakages could not be *undone* — the text it wrote
happened to appear elsewhere in the file, so the automatic restore refused to
act. The next breakage was therefore measured against hardware that was still
broken from the previous one, and scored as a success. The run ended with the
"clean" check failing, which is how I noticed.

The existing safeguard could not see this: it checks that the compiled file
*changed*, and it always does. So the harness now makes a **second** check —
that the restore actually happened and left the file byte-identical to a copy
taken before the run — and stops the whole sweep if it did not, rather than
carrying on producing numbers nobody should trust. Re-run properly, one of the
"successes" turned out to be a false one.

**A ring has a line of dead code, and I left it in.**

One breakage survives no matter what, and the reason is arithmetic: the midpoint
between two radii can never overflow, whatever the radii. I checked that over
300,000 cases. So the line that would report an overflow there can never fire.

I left it in and wrote down why. The reference does it that way, and this
hardware exists to agree with the reference — not to be cleverer than it. The
day someone widens those numbers, a version that had "tidied it away" would be
silently wrong.

### Where the hardware stands

**37 specified · 4 reference-complete · 36 unit-verified · 14 rtl-verified**
across **91** blocks.

Still waiting on your call about the frame blitter's timing
(`reports/BLIT_INTEGRATION_PHASE_SHIFT.md`) — it is built, works, is faster, and
shifts a phase the demo pins. That one decision gates the rest of the
integration and the resource re-fit you asked for.

---

## 2026-08-21 — the arbiter is in the machine, and the gate test had been red for three days

### The arbiter is wired into the shell

The bridge arbiter now sits in the real `zhao_shell_top`, between the command
path and the host-memory bridge. The blitter's side is tied off until it is
wired — the point of putting it in **before** the blitter exists is that when
the blitter arrives, a regression has only one possible cause.

It does real work already. The command path and the bridge disagree about how
long a request stays up: the command path **pulses** it for one cycle. My
arbiter re-read the port a cycle later, so it would have **lost every request
the command path ever made**. Nothing found that until it was actually wired
into the shell, which is the argument for wiring things early rather than at the
end. It now captures the request on the edge it picks a winner.

**The 600-frame gate test passes: 24,630 of 24,630 checks.** Every frame is
bit-identical.

### But it did not pass at first, and the reason was not mine

The gate test failed on the very first run — one check out of 24,630: the
captured file was not byte-identical to the committed reference.

I did not regenerate the reference. I diffed it. **Sixty-nine bytes differed,
and every one of them was inside a small block that records which version of the
data format the build was compiled against** — a version number and two
fingerprints. Nothing the simulation can influence. Every frame checksum, every
controller reading, every counter: identical.

So the reference file has been **stale since 18 August**. It was regenerated in
the same commit that bumped the format version, and the regeneration ran against
a build that had not picked up the change — so it recorded the *old* version
number and the *old* fingerprints. It has disagreed with the code ever since.

**Nobody noticed for three days because that test takes twenty-two minutes and
only runs nightly.** A check that expensive is a check that runs rarely, and one
that runs rarely is one whose failures age quietly.

### So I made that specific failure cheap

The stale block is built entirely from compile-time constants. Checking it does
not need a twenty-two minute simulation — it needs to open the file and compare
five numbers. There is now a test that does exactly that, in the fast lane, over
**every** committed reference capture. It takes under a second.

I verified it the only way worth verifying such a thing: I put the stale file
back and confirmed it fails, naming all three wrong fields. Then I put the good
one back.

It does not replace the full gate test. It separates the one failure mode that
has nothing to do with the hardware from the many that do.

### Where the hardware stands

**37 specified · 4 reference-complete · 36 unit-verified · 14 rtl-verified**
across **91** blocks.

Step 4 is partly done — the arbiter is in. Still to wire: the blitter itself,
the slot manager into frame control, and the write queue's back-pressure. Then
Step 6 deletes the old blitter from the command path, and Step 8 is the composed
re-fit you asked for.

---

## 2026-08-21 — the bridge arbiter, and a rule I had exactly backwards

Step 3 done: **`MEM.HPS.ARBITER`**, block 91. Two things now want the one
connection to the host memory — the command path and the frame blitter — and
this decides who gets it.

It is tested **wired to the real bridge**, not to a stand-in. That mattered
immediately, and it is the same lesson as last time: the real bridge refuses
things a stand-in would have waved through.

### The rule I got backwards

Your review says the arbiter should hold its request steady until the bridge
accepts it. That is right for the side facing the clients and **exactly wrong**
for the side facing the bridge: the bridge takes the request instantly and only
*says so* a cycle later, so a request still sitting there when the acknowledgement
arrives counts as a second request — and gets rejected. Every single transfer
would have failed.

The command path already did this correctly, and had for months. Converting
between the two conventions is now the arbiter's main job.

### The mutation sweep found a real bug

One deliberate breakage **survived**: routing *write* data from the wrong
client. It survived because no test wrote anything. Chasing that found something
worse than a missing test — **the bridge answers a write with no reply at all**,
so my "is it finished?" condition would have waited forever on the first write
the machine ever did. Nothing writes to host memory yet, which is precisely why
it was worth fixing now: a path that is untested *and* broken looks finished.

### Something for you to decide

The command path has strict priority over the blitter. That guarantees the
command path never waits. It guarantees **nothing** about the blitter ever being
served — if the command path asked continuously, the blitter would never run.

In practice it asks about once a frame, so it is fine. But "fine" here depends on
someone else's behaviour, not on anything the arbiter enforces. So the test pins
both halves: the priority works, **and** the starvation is real and measured.

If you would rather it were fair, the fix is small — after a command-path
transfer, let a waiting blit go next, costing the command path one blit's worth
of delay. Your call; I have left it as you specified.

### Where the hardware stands

**37 specified · 4 reference-complete · 36 unit-verified · 14 rtl-verified**
across **91** blocks.

Steps 1, 2 and 3 of your list are done. **Steps 4 to 8 are not** — wiring the
real memory path, publication into frame control, deleting the old blitter from
the command path, shell tests, and the composed re-fit. Still not in the running
machine.

---

## 2026-08-21 — the slot manager, and the thing nothing was deciding

Step 2 of your integration list is done: **`VIDEO.SLOTMGR`**, the ninetieth
block.

### What it is, and why it did not exist

The frame blitter writes into a screen buffer *before* it knows whether the
picture is any good, and only makes it visible if everything checked out. That
is safe for exactly one reason: the buffer it scribbles into is not the one you
are looking at.

**Nothing in the machine was actually deciding that.** The shell handed out a
write permit when a blit started and took it back when it finished — no record
of which buffer was on screen, no way to tell two permits apart, and no way to
refuse an instruction that arrived too late. The whole safety argument rested on
a piece that was never built.

This block is that piece. It tracks each buffer as free, being written, ready,
or on screen, and it stamps every permit with a number so a blit that lost its
permit and one that never lost it can be told apart. That distinction is the
entire point — without it they look identical at the moment they ask to be shown.

68 directed checks, 28,290 randomised, **14 of 14 deliberate breakages caught**,
and the formal proofs pass with all eight reachability checks reached.

### The proofs found a real bug

If a blit said "show this" and "throw this away" **on the same clock edge**, the
two instructions raced and the later one silently won — so a buffer could be
thrown away on the very edge it was told to be shown.

The blitter is proven never to say both. But this block is the *authority* on
who owns a buffer, and an authority that only works while everyone else behaves
is not an authority. It now refuses the pair outright.

Two more findings were in the proofs rather than the hardware, and I mention
them because they are the same species as last time: a way of writing "what was
true one cycle ago" that reads correctly and means something else, and a check
that recorded events happening *during* reset, which the hardware correctly
ignored. Both made a proof look like it was watching something it wasn't.

### Where the hardware stands

**37 specified · 4 reference-complete · 35 unit-verified · 14 rtl-verified**
across **90** blocks.

Steps 1 and 2 of your list are complete. **Steps 3 to 8 are not started** — the
bridge arbiter, the real memory wiring, publication into frame control, deleting
the old blitter from the command path, shell tests, and the composed re-fit. The
new path is still not in the running machine.

---

## 2026-08-21 — your frame-blit review: you were right, and it was worse than one bug

I read `DEBUG.FRAMEBLIT_Integration_Corrections.md` and checked the claims
against the code instead of just accepting them. **They hold.** The three I
could verify directly:

- **Slot 1 was broken outright.** The memory guard only accepts a write whose
  address is inside the leased slot, and slot 1 starts at 0x02000000. My block
  was sending the offset from the start of the slot, not the real address. Slot
  0 worked *only* because slot 0 starts at zero. Every single slot-1 blit would
  have been rejected.
- **"The writes finished" was a lie my block told itself.** It counted a chunk
  as complete when it handed it downstream, not when the memory actually wrote
  it. So a picture could go on screen while part of it was still in a queue. The
  real signal existed the whole time — the memory arbiter reports completions —
  and I just wasn't using it.
- **It never waited for the memory bridge to say yes.** It asked and carried on.
  That works only while nothing else is using the bridge, which stops being true
  the moment the command path shares it.

My full point-by-point answer is in
**`reports/DEBUG.FRAMEBLIT_Integration_Response.md`**.

### The thing I actually want to tell you

All six problems have one cause, and it is not really about this block.

**Every fake I built to test against was more agreeable than the real thing.**
The fake memory guard approved writes without ever looking at the address. The
fake bridge said yes instantly. The fake memory reported writes as finished the
moment they were handed over. So the test passed 43 checks against a block that
was wrong six ways — not because the checks were weak, but because nothing in
the room was capable of saying no.

I have rebuilt the fakes so they can refuse: the guard checks addresses and can
be slow, the bridge makes it wait, the memory can withhold completions or freeze
them forever. That is the change I would want applied to the other blocks too,
and I have added it to the list.

### What is done

**All of Step 1, formal proofs included.** Absolute addressing, real
retirement, drain-before-release, publish/release carrying slot and generation,
pre-acquisition failures releasing nothing, the lease checked at the exact
publication edge, and the bridge grant.

97 checks, up from 43. Mutation sweep on the thirteen defects your review names:
**11 caught, 2 provably-cannot-differ, 0 unusable results.**

The proofs are worth a sentence. There are 27 of them and they hold, but the
part I actually care about is that **all 8 "can this even happen" checks are
reachable**. A proof that a picture is never shown wrongly is worthless if the
machine being proved can never show a picture at all — it passes by never
getting there. That has already happened twice in this repo, so the lane now
proves reachability first and the property second.

### What is not done, plainly

**Steps 2 through 8** — the slot manager, the bridge arbiter, wiring the real
memory path, removing the old blitter from the command path, shell tests, and
the composed re-fit. The standalone block is correct in isolation and **is still
not in the running machine**, exactly as your point 8 says.

I have taken your Step 8 instruction as binding: no new large feature before the
composed resource result. That result needs Step 6 first, so the order stands.

---

## 2026-08-21 — the table ops, and a sweep that was lying to me

### The Field IR engine

`CURVE`, `DCURVE` and `SPLINE` are built and verified. These are the ops that
read a **curve table** — the compiled shape you get when a designer draws a
falloff, an envelope or a path and it gets baked into the program.

There is now a report for the whole engine so far, so the numbers live somewhere
citable instead of only in commit messages: **`reports/FIELD_IR_ENGINE.md`**.

Still simulation. Nothing here has run on a board.

### The part worth your attention: the sweep was reporting scores for tests that never ran

The way each piece gets finished is that I break the hardware on purpose,
eighteen different ways, and check the test notices. That is the only thing that
tells me a passing test is actually looking where the bug would be.

This time nine of the eighteen came back as "no result". The build system was
handing my test an **old copy** of the hardware after I had changed it — so the
test was passing against the previous version and calling it a pass. At one
point it did this with the *correct* version too, which is how I noticed.

The sweep already refused to score a run where the compiled file had not changed
— that check is why this surfaced at all rather than becoming eighteen out of
eighteen. It now rebuilds from scratch every time, which is slower and honest.

I mention it because it is a general hazard, not a one-off: any result in this
repo that came from an incremental build is worth a second look.

### And three real holes it found

Once the sweep was trustworthy, it found three places my test was not looking:

- A safety clamp could be **deleted entirely** and every test still passed —
  because I only ever tested well-formed tables. The program format lets a table
  through with one field unvalidated, and that clamp is the only thing standing
  between that and a disagreement with the software.
- A whole half of another clamp was invisible, because of an accident in how my
  test built its tables.
- A rounding law was only tested where it could not fail.

All three are closed. The directed test now catches all eighteen on its own.

### Where the hardware stands

**37 specified · 4 reference-complete · 34 unit-verified · 14 rtl-verified**
across **89** blocks. `ctest -L fast`: **239/239**.

Next in the engine: `NOISE2`, `RING`, `RIDGE`, `ROT2`, `ROT3`, then the
sequencer that actually runs a program. None of that needs anything from you.

**What still does need you** is unchanged, and it is the larger half: roughly a
dozen blocks — particle spawn/age/collide, bloom and heat-haze, the HUD sprite
pipeline — have no software to copy and no spec section. I can invent the
behaviour, but the invention becomes the law the hardware is built to.

---

## 2026-08-22 — the frame blit is out of the command front end

### Where the hardware stands

**37 specified · 4 reference-complete · 34 unit-verified · 14 rtl-verified**, now
across **89** blocks — one more than before, because DEBUG.FRAMEBLIT is a new
block rather than a rewrite. `ctest -L fast`: **239/239**.

### DEBUG.FRAMEBLIT — you were right, the design was sitting there

`reports/CMD.DMA_Redesign_Proposal.md` Part 2 had it written up and nobody had
built it. It is done now.

The reason it matters is the one that was blocking the whole shell from fitting:
the debug frame blit used to keep **a whole screen's worth of pixels** in a
staging buffer inside the command front end, because the old rule said nothing
may be written to video memory until the checksum has been verified. That single
buffer is what made the design too big.

The rule is now amended, and it is an amendment rather than a loosening:

> Nothing becomes **visible** until every byte is written and the checksum
> matches.

Which is sound, because writing into a screen buffer nobody is looking at is not
visible to anyone. So the screen buffer itself does the job the staging buffer
was doing — that is what double buffering is *for* — and the staging buffer drops
from about two million bits to **512**.

Two details worth knowing:

- **A failed blit leaves a dirty buffer behind, on purpose.** It is never shown.
  The test asserts that rather than tidying it away.
- **The buffer is now leased**, and the lease is checked every single cycle. The
  compiler flagging one unused signal led to a real hole: a lease that drops and
  is immediately re-granted for the *same* buffer looks exactly like one that
  never lapsed, while the bytes already written belonged to somebody else. That
  is closed.

### Website

Updated and live at **zhaozhou.pages.dev**. The ledger figures, the creature
path, the projection block, the spell-program engine and an honest "what is left
and who it is waiting on" section are all on it now. Phases 6–10 were showing
"not started" while actually holding verified blocks; that is fixed.

Two things from that pass you should know:

- **`deploy.ps1` had a bug**: it passed a `--branch` flag that was never declared,
  so it expanded to nothing — which is exactly the silent "deployed as a preview
  instead of production" failure its own comment warns about. Fixed. `update.ps1`
  still has the same shape and was left alone.
- Several good numbers were **left off the site on purpose** because they only
  exist in this file or in commit messages, not in a committed report: the pose
  cache's 192 KB, the multiplier counts, the cycles-per-pose figures. If you want
  them public they need a line in a report first.

No new images. Those wait until the hardware can render end to end, as you said.

### Also running

An **hourly reminder** is set so I pick this up again if I stop. It carries the
full working method, not just "continue" — check the reference exists first,
mutation-sweep everything, discard any result where the binary did not actually
rebuild. It lives only in this session, so if the session ends it goes with it.

### Engine progress

Length, distance and normalise-support landed: an exact integer square root,
binary longhand, thirty-two fixed steps. Fixed latency on purpose — nothing
downstream has to model a variable delay.

---

## 2026-08-21 (late) — three more blocks, and an honest scope note

### Where the hardware stands

**37 specified · 4 reference-complete · 33 unit-verified · 14 rtl-verified.**
`ctest -L fast`: **214/214**, up from 180 this morning. All pushed.

New since the last note: **GEOM.PROJECT** (creature vertex → pixel position),
**FIELD.PROGCACHE** (keeps validated spell programs resident), **PART.EXPAND** and
**PART.SOFT** (the two ways a particle becomes something drawable — a triangle for
big ones, a point sprite for small ones).

### A technique that has earned its keep twice today

For the two particle blocks the law was **buried inside the software renderer** —
computed and immediately drawn, with no function to point at. So I had to copy it
out, and a copy that only I ever compare against the original is worth nothing.

Both tests therefore draw the same particles **twice**: once through the real
renderer, once through my copy, and demand the two pictures match pixel for pixel.
If my copy ever drifts from the renderer, that fails — and it fails whichever of
the two moved. That check is what makes the rest of the test mean anything.

### What I got wrong today, and how it surfaced

Three times, breaking the hardware on purpose caught a *comment* that was wrong
rather than code:

- I claimed a division in the particle expansion truncates and that odd sizes
  would expose a rounding error. **It never truncates** — the numbers are always
  multiples of 16. No test can tell the two apart. I'd written that claim in three
  files; all three now say so.
- I claimed a certain shift had to be arithmetic or sprites would vanish. **At
  these widths it makes no difference.** Corrected.
- The sprite test **could not tell whether the block was reading the right edge of
  an offset viewport** — every test sprite sat left of where the two answers
  differ. That is the same latent bug the software renderer has a fixed-note
  about: it made the second player's view silently invisible for a while. Now
  covered.

### The scope note — this matters for planning

I checked every remaining block for a law already written down somewhere under a
different name. **There are no more.** The four I finished today were the last of
the cheap ones.

The remaining 37 split three ways, and only one is mine to do alone:

1. **Needs a spec another part owns** — the compressed vertex format has two
   ends, and the packing end belongs to the asset tools. Inventing one end on my
   own would create exactly the kind of made-up law we keep catching.
2. **Needs the spell-program engine** — five blocks, one engine. Big, but
   completely unambiguous, and it needs nothing from you. **This is the next real
   thing I can do.**
3. **Needs you to decide how the game behaves** — roughly a dozen blocks:
   how a particle spawns, ages and collides; what bloom and heat-haze look like;
   what the HUD sprite pipeline does. There is no software to copy and no spec
   section to follow. I could invent it, but then my invention becomes the law
   the hardware is built to, and that should be your call rather than mine.

So: "finish the hardware" is not one more sitting. I can keep going on group 2
indefinitely. Group 3 is waiting on you, and it is worth a conversation rather
than me guessing.

**Group 2 is under way.** Three pieces of the spell-program engine are built and
verified:

- **the arithmetic core** — fifteen operations (add, multiply, compare, clamp,
  dot product…), 300,000 random checks clean;
- **the reciprocal** — division, which needs a 256-entry table and a correction
  step, 150,000 checks;
- **sine and cosine** — checked **exhaustively**: an angle here is a 16-bit
  number of turns, so all 65,536 of them, both functions, every one compared.
  Not sampled. That one is a proof rather than evidence.

Both tables are **generated from the reference by a script, not typed in**, and
then checked entry by entry — 256 and 257 constants respectively, and a wrong one
wouldn't fail loudly, it would just put a few angles slightly off.

Still to come in the engine: length, distance, normalise, the curve/spline/noise
ops, and then the sequencer that runs the program. Each is its own piece.

One thing the engine does deliberately: an operation it cannot yet perform
**refuses** rather than returning zero. A quiet zero would let it run a spell it
cannot actually evaluate, and everything would look fine.

### Four times today my own comments were wrong

Every one was a claim I'd just written about why something matters, and every one
was caught by deliberately breaking the hardware rather than by re-reading:

- a division that "truncates" — it never does, the numbers are always multiples
  of 16;
- a shift that "must be arithmetic or sprites vanish" — at these widths it makes
  no difference;
- "multiply-then-round is off by one" — it's *exactly* equal except at the
  extremes;
- a table guard I called an X-propagation hazard — it isn't; the real protection
  is elsewhere and the guard is redundant.

None of the four changed the hardware. All four would have misled whoever read
the file next, including me.

---

## 2026-08-21 (night) — the backlog is empty; real building starts

### Where the hardware stands

**39 specified · 4 reference-complete · 31 unit-verified · 14 rtl-verified.**
`ctest -L fast`: **208/208**, up from 180 this morning. All pushed.

Two greenfield blocks tonight: **GEOM.PROJECT** (below) and **FIELD.PROGCACHE**,
the cache that keeps validated spell programs resident so they are not
re-checked every time they are cast. Its rule is the strict one — a program that
fails validation is rejected and *never* runs — and it is tested with the three
real compiled programs we have, plus deliberately corrupted copies of them, so
the rejection cases are the loader's own verdicts rather than a flag I made up.

Today moved seven blocks: the four creature-animation ones, the trace ring, the
scar store, the terrain bake and velocity blocks, and tonight **GEOM.PROJECT** —
the block that turns a posed creature vertex into a pixel position on screen.

### The important finding: there is nothing left to tidy up

I checked every one of the 24 remaining blocks by trying to advance each one and
recording what the ledger objected to. Full map in
`reports/REMAINING_BLOCKERS.md`.

**Several blocks today were finished months ago and simply never recorded.** That
is now exhausted — `TERRAIN.BAKE` was the last one, and it advanced with no work
at all beyond regenerating a diagram.

What is left is sixteen blocks that **do not exist yet**, plus five waiting on the
Field IR sequencers, plus one deliberately postponed. So "get through the waves"
from here means writing sixteen blocks, not clearing a backlog. I want that to be
plain rather than discovered slowly.

### GEOM.PROJECT, and why it was the one to do first

Its declared reference didn't exist — but the real law did, under another name,
and the terrain side is already verified against it. So it was the cheapest of
the sixteen and a good pattern for the rest.

The interesting part is the divider. Projecting a vertex needs a division, and it
has to be **exactly** the division the reference does, down to the last bit —
including how it rounds negative numbers, where the obvious approach is subtly
wrong. There is no shortcut: no reciprocal trick reproduces it. So the block
contains a real 31-step divider, pipelined so it still does one vertex per clock.

### Two more tests that were lying, both found by breaking the hardware

Same story as this morning, and I don't think it is a coincidence any more:

- The dual-view test **could not tell whether the block was reading the right
  camera.** Our two views happen to differ only vertically, so a version that
  always read camera 0 horizontally gave identical answers and passed. Fixed with
  a second pair of views that differ both ways.
- The rounding test **passed a version that rounded wrong**, because every matrix
  I had written was made of whole numbers with nothing to round. Exactly the
  mistake I made in the skinning block this morning, made again eight hours
  later.

Both now fail loudly. The mutation sweep is the only reason either was found, and
it has now caught something in **five of the seven** blocks I finished today.

### Nothing needed from you

Except the pose-cache memory question from earlier — still open, still not
blocking anything.

---

## 2026-08-21 (evening) — two more blocks done, and one I refused to fake

### Where the hardware stands

**43 specified · 4 reference-complete · 27 unit-verified · 14 rtl-verified.**
`ctest -L fast`: **200/200**, up from 180 this morning. Everything pushed.

Today added, all differential against the shipped reference, all with mutation
sweeps: the four creature-animation blocks (earlier note), the **trace ring**,
and **SURFACE.SHEET** — the store that holds the scars burned into the ground.

### The scar store was finished months ago and stuck anyway

SURFACE.SHEET had working hardware and a good test suite, and could not advance,
because the ledger looked at its test and said *"that is an alias, not evidence."*

It was right. The test imported the reference model, named it, and then never
called it. It checked that the block agreed with **itself**.

The differential I wrote runs the real hardware and the real reference over one
stream of operations and makes them agree on every one of 4,096 texels. The law
it defends is the one the whole block exists for: **re-acquiring a patch does not
wipe it.** A scar a player burned into the ground stays there. An implementation
that cleared on every acquire passes any test that writes and reads in one go —
and erases the world between frames. That version now fails.

### The thing I did not do

Advancing that block made a rule demand four more tests. One was real and I wrote
it. **The other three describe behaviour that exists in no block at all.**

The op list says TERRAIN.PATCH writes material, navigation and hazard data. It
does not: no ports for it, nothing in the reference, and its own contract says in
so many words that field-program work belongs to a different block — one nobody
has built yet.

I could have deleted three lines from a config file and turned everything green
in about a minute. I didn't, and I want to be explicit that this was a choice:

- it might simply be wrong — that list may mean "will own", not "does own", and
  the block's stated purpose does claim those layers eventually;
- and even if it were right, **editing a rule's input until the rule stops
  complaining is not the same as satisfying it.** That is the exact move that
  produced the "alias, not evidence" problem in the paragraph above.

So TERRAIN.PATCH stays marked unfinished. That is the true state, not a chore I
skipped.

### An audit worth having: 25 named references do not exist

I have now hit this nine times one at a time, so I checked all of them at once.
**Of 73 declared reference models, 25 name something that exists nowhere in the
codebase.** Full list in `reports/PHANTOM_REFERENCES.md`.

None of it is broken — the ledger is correctly refusing to accept a name for
nothing as proof. But it changes what each remaining block costs, because they
fall into three different kinds:

1. **The law exists under another name** — just point at it. Cheap.
2. **The law was never written** — someone has to decide the open questions and
   write them down once. That was the trace ring today.
3. **The idea is wrong for that block.** The clock generator, the reset
   sequencer and the clock-crossing block are the clear cases: a PLL has no
   scalar model to compare against. Their correctness is a *timing* property. The
   ledger has one slot for "what proves this block right" and it does not fit
   them, so they either carry a fiction or can never advance. Worth fixing in the
   tooling before those blocks come up, not during.

### Still waiting on you

The pose-cache memory question from the earlier note (192 KB, ~28% of the chip's
fast memory). My recommendation is unchanged: pack by real bone count **and**
cache fewer poses. Nothing is blocked on it today.

---

## 2026-08-21 (later still) — the creature path now exists in hardware

### What moved

Four new blocks, all differential against the shipped reference, all with
mutation sweeps:

| | |
|---|---|
| `quat2mat` | quaternion to rotation matrix — the innermost step of posing a bone |
| `mat3x4_mul` | the chain multiply, **sequential** (see below) |
| `pose_decode` | the whole per-bone chain: one clip + frame in, a full skeleton pose out |
| `pose_cache` | which poses are already decoded, and which one to throw away |

Together with GEOM.SKIN from earlier today, that is the **entire creature
animation path** in hardware: bones pose, poses chain down the skeleton, vertices
follow the bones. This is the road to the animation-state inventory — every state
in it eventually becomes a bone palette, and this is the hardware that runs one.

`ctest -L fast`: **192/192**. Everything pushed.

### I spent DSPs where it mattered and refused to elsewhere

The chain multiply, done the obvious way, is **36 multipliers**. Twice what
skinning costs, on a board with 112 where we already want 171.

I built it to do **one element per cycle with three multipliers** instead —
twelve cycles per matrix. Then the whole decode chain shares **one** multiply
engine across every bone, so the entire creature-posing block costs **three
multipliers, not seventy-two.**

It is slower, and that is fine here, because posing is not per-vertex work. A
pose is decoded once per (creature type, animation, frame) and then **shared by
every one of that creature on screen that frame** — that is the whole reason the
cache exists. A full 32-bone skeleton takes about 1,600 cycles to pose. At 50 MHz
a frame is 833,000.

This is the first of the savings I listed as "open" this morning, actually taken.

### One decision I want from you

**The pose cache needs somewhere to keep 128 poses. That is 192 KB — about 28% of
all the fast memory on the chip, for one feature.**

I did not spend it. The block I built decides *which* pose is needed and *where*
it goes, and hands the storage question back out. Burying a quarter of the chip's
memory inside a module is how a budget gets spent without anyone choosing.

Four ways out, cheapest first:

1. **Pack by real bone count.** 32 bones is the maximum, not the typical. A
   12-bone creature would use 12 bones' worth. Probably a 2–3x saving for free.
2. **Cache fewer poses.** 32 instead of 128 is 48 KB (7%). The risk is more
   re-decoding when lots of different creatures animate at once.
3. **Keep poses in main memory.** Costs bandwidth every time a creature is drawn
   instead of chip memory once.
4. **Store poses smaller** (rotation + position instead of a full matrix): 8 bytes
   a bone instead of 48. But then skinning has to rebuild the matrix, which costs
   the multipliers we just saved.

**My recommendation: 1 + 2 together.** They compose, they cost nothing at runtime,
and between them the cache drops to a few percent of memory instead of a quarter.
4 trades the resource we are short of for one we are not, so I would not.

Tell me which and I will build it. Until then GEOM.POSE stays honestly marked
unfinished — two verified halves are not a finished block.

### Two more times the tests were lying

Same lesson as this morning, twice more:

- The cache eviction test **could not tell "throw away the oldest" from "throw
  away the newest."** It checked both groups, so both policies gave identical
  totals with the roles swapped. A backwards cache passed it. Fixed; the
  backwards version now fails 4 checks.
- Two other cache sections asked for animation frames **past the end of the
  animation**, so those requests were rejected as invalid and the sections tested
  nothing at all. They looked green the whole time.

I keep finding these by breaking the hardware on purpose and checking the test
notices. It is the only reason I trust any of the green numbers above.

---

## 2026-08-21 (later) — GEOM.SKIN built; the test nearly shipped a lie

### What moved

| | |
|---|---|
| `GEOM.SKIN` | SPECIFIED to **UNIT_VERIFIED** — RTL, contract (15 sections), 2,125 directed + 24,000 random checks |
| phantom #10 | `zref::Skin` was the declared oracle. It does not exist anywhere in `reference/` |
| `GEOM.CLIP` | evidence path corrected — it cited the wrong file |

Skinning is how a creature's vertices follow its bones. It is the block the
animation-state work leads into: every state in the inventory eventually becomes
a bone palette, and this is what turns that palette into a moving creature.

### The thing worth your attention

**My first version of the test passed completely, and was nearly worthless on the
one law it existed to defend.**

The law is *single rounding*: the blend of two bones is computed exactly and
rounded once at the end, never rounded twice along the way. Double rounding is
off by one unit in the last place — invisible as a bug report, visible on screen
as a silhouette that shimmers while a creature moves.

So I broke the RTL on purpose and re-ran. **The double-rounding version passed all
39 of my directed checks.** It failed one random case in three hundred, by one
LSB. If I had not run the sweep, this block would be sitting at UNIT_VERIFIED
with a test that green-lit the exact defect it was written to catch.

The cause was mundane: every matrix in my test was built from whole numbers, so
there was nothing below the rounding point to lose, and rounding early threw away
nothing. Real bone matrices are not whole numbers.

The fix is a section that sweeps all 63 weights against eleven awkward vertex
values using deliberately ugly matrices. That mutation now fails **696 of 2,125**
checks instead of one in three hundred.

A second one: zeroing the vertex's ID tag passed all 2,118 arithmetic checks,
because all of them compared coordinates and nothing else. That would have sent
every vertex to the wrong creature. Also now covered.

Final sweep: **ten deliberate breakages, ten caught.**

### One process note

Verilator silently skipped rebuilding several times — the test binary was
byte-identical after I changed the RTL, so I was reading old results. I now check
the binary's hash after every rebuild and **discard the result** if it did not
change. Three mutation results were thrown away and re-run for this reason. It is
worth knowing that "the tests passed" is only meaningful if the tests were
actually rebuilt.

### Honest about the cost

GEOM.SKIN needs **eighteen 32x32 multipliers** in its blend path. The board has
**112** DSPs; the project already accounts for **171**. This block makes the DSP
problem worse, and I have not run a fit on it, so I am not going to quote a
number I do not have.

The contract lists the ways to cut it (share one row engine across three cycles;
share one engine across both bones; exploit that a rotation's elements are small)
— all unexercised. Filed as evidence for the budget argument, not as a solved
problem.

### Nothing needed from you

`ctest -L fast`: **180/180**. Ledger green. Both commits pushed.

---

## 2026-08-21 — waves worked in order; the ledger argued back, correctly

### What moved

| | |
|---|---|
| `CMD.DECODER` | SPECIFIED to **UNIT_VERIFIED** — contract, oracle, RTL, differential, random lane, mutation sweep |
| `DEBUG.TRACE` | SPECIFIED to **REFERENCE_COMPLETE** |
| line buffer | now infers as block RAM; formal never-torn proof still passes |
| 16 blocks | SPECIFIED to **REFERENCE_COMPLETE** (they were already built) |
| 5 FIELD.STAMP ops | first differential coverage the op layer has ever had |

Ledger: **47 SPECIFIED, 23 REFERENCE_COMPLETE, 4 UNIT_VERIFIED, 14 RTL_VERIFIED.**

### The finding worth your attention

**Twenty-two blocks were sitting at SPECIFIED while already having RTL, a
resolving oracle and passing tests.** They have been synthesized and tested all
session. Only the ledger maturity was never advanced, so the dashboard has been
badly understating what exists.

When I advanced them, **the ledger refused seven**, and each refusal was real:

- **V17 "an alias, not evidence"** — `TERRAIN.VELOCITY`'s random test never
  mentions `velocity_vertex`; `SURFACE.SHEET`'s never mentions `SheetStore`;
  neither `RASTER.EDGEWALK` test mentions `EdgeWalk`. A test that exists but is
  not *about* the cited oracle is not evidence the oracle is implemented.
- **V10** — advancing `TERRAIN.PATCH` and `TERRAIN.BAKE` made eleven FIELD ops
  active, and every declared differential test was missing.

I would have advanced all 22 unchallenged. The rules caught it.

### Where the remaining six FIELD ops actually stand

`FIELD.OUT.{HEIGHT,VELOCITY}` and `FIELD.WRITE.{MATERIAL,NAV,TAG,HAZARD}` still
have no differential test, and **I did not write one, deliberately.**

They are output ROUTING declarations, not computations. `FIELD.OUT.HEIGHT`'s own
semantics line says *"profile output map — no dedicated opcode"*. The zfield
interpreter routes `out_lanes` generically; there is no distinct behaviour these
six add that a test could compare against a second implementation, because
**their implementing block `FIELD.SEQ.EARTH` does not exist** — it was refused
earlier in this project as a block-sized project in its own right.

So six hollow test files would have turned a true red into a false green. The
ledger is correctly saying: **`TERRAIN.PATCH` and `TERRAIN.BAKE` cannot be
reference-complete while the ops they implement have no verified routing**, and
that unblocks when `FIELD.SEQ.EARTH` is built, not before.

### Still true

Phase 0 is entirely `blocked_on: hardware`. Phases 3–11 are unstarted. **Nothing
has run on a programmed device.** The `CMD.DMA` redesign in
`reports/CMD.DMA_Redesign_Proposal.md` is still the gate on a composed fit, and
the `blit_buf` design question there is still yours to decide.

---

## 2026-08-20 08:15 — read the redesign proposal. It is right and my answer was wrong

`reports/CMD.DMA_Redesign_Proposal.md` arrived on origin. I have read it in full.
**Adopt it.** It is better than what I proposed, and it identifies a soundness
hole in my suggestion that I had not seen.

### Where I was wrong

I recommended **two passes over DDR** — read once to CRC, read again to commit —
on the grounds that it costs no on-chip memory. The proposal points out that
this is **not sound**: the pixel arena is a raw HPS address with no descriptor,
lease or ownership state, so HPS can mutate it between the two passes.
Pass 1 verifies bytes A, pass 2 commits bytes B, and the CRC certifies nothing.
A classic time-of-check/time-of-use hole, and I walked straight into it.

Making two-pass sound would first require a sealed pixel-arena descriptor that
forbids HPS writes in between — inventing an ownership mechanism to rescue a
design that then still costs double the bandwidth.

### Why its answer is better

It moves the atomicity boundary to where it actually is. The current law says
*no byte is written to VRAM before the CRC passes*, which is what forced a
whole-canvas buffer. But **writes to an inactive, uncommitted framebuffer slot
are not visible to anyone.** The externally meaningful commit is the slot
becoming READY.

So: lease an invisible slot, stream into it one 64-byte chunk at a time, CRC on
the fly, and publish READY only after every write has retired and the CRC
matches. Fail, and the slot is released FREE and never published.

**512 bits of buffer instead of 1,966,080.** Single pass. And it is immune to
the mutation problem — if HPS changes the source mid-read, the CRC fails and the
dirty slot is never shown.

That is the better argument, and it wins on correctness rather than on cost.

### What tonight's synthesis run adds to it

The run finished after the proposal was written, so this is new evidence for it,
and it confirms two of its rules from the tool's own mouth:

- Quartus named **asynchronous read** as the *only* stated reason `blit_buf` is
  uninferred. The proposal's synthesis rules list "no combinational array reads"
  — that is precisely the one that bites.
- **A second block has the identical defect**, outside CMD.DMA entirely:
  `zhao_scanout_linebuf` line 96. The proposal's rule list should be applied
  there too; it is not a CMD.DMA-specific mistake but a house-wide one.
- `zhao_audio_fifo` **did** infer, as a dual-clock RAM, carrying the warning
  that read-during-write behaviour of a dual-clock RAM is undefined and may not
  match the original design. Worth checking separately — that is a correctness
  risk wearing a success.

### Its other two findings, which I had not looked for and which look right

- **The contract claims 1 MiB packets; the RTL caps at 4 KiB** and rejects
  anything larger. There is already a hidden effective maximum.
- **The 1 MiB-per-frame target is physically impossible.** 16,384 bursts at
  (16 + 8) cycles is 393,216 GPU cycles against 217,984 in the shortest mode
  frame. The HPS bridge cannot deliver it regardless of what CMD.DMA does. That
  belongs in the same category as the DSP overrun: a number nobody costed.

### What I have and have not changed

The 64-bit repack I landed (`3a97980`) stands as the emergency cleanup it is:
16.2 GB to 2.7 GB elaboration, all lanes green, and it is what let the composed
synthesis produce a named error at all. It does **not** solve the design
problem, and the proposal says so correctly.

I have **not** started the redesign. It splits a landed block in two, adds a
framebuffer-slot lease with a real state machine, changes the decoder-facing
stream from 8 to 64 bits, and amends a ratified law. That is a wave of work and
it should start deliberately.

---

## 2026-08-20 08:00 — THE COMPOSED SYNTHESIS RUNS. 28.4 GB to 6.2 GB, and Quartus names the fault

The composed shell no longer thrashes. It ran analysis and synthesis to
completion and **failed with a precise, named error** instead of dying:

```
Peak virtual memory: 6352 megabytes      (was 28,400)
Elapsed: 42:33

Info (276014): Found 2 instances of uninferred RAM logic
  Info (276007): RAM logic "zhao_cmd_dma:u_dma|blit_buf" is uninferred
                 due to ASYNCHRONOUS READ LOGIC ... line 239
  Info (276007): RAM logic "zhao_video_scanout:u_scanout|
                 zhao_scanout_linebuf:u_linebuf|mem" is uninferred
                 due to ASYNCHRONOUS READ LOGIC ... line 96
Error (276003): Cannot convert all sets of registers into RAM megafunctions.
  The resulting number of registers remaining in design exceeds the number
  of registers in the device.
```

**This is the whole answer, from the tool, in its own words.** Not a guess.

### What it tells us that we did not know

1. **The fix is SMALLER than I said.** I wrote that `blit_buf` needed both the
   async reset removed *and* a registered read. Quartus lists **only the
   asynchronous read**. One change, not two.
2. **A SECOND block has the identical defect.** `zhao_scanout_linebuf` line 96,
   `logic [63:0] mem [0:1][0:127]`, same cause. It was never going to infer
   either, and nobody knew.
3. **`zhao_audio_fifo` DID infer**, as a dual-clock RAM — with a warning worth
   following up: *"the read-during-write behaviour of a dual-clock RAM is
   undefined and may not match the behaviour of the original design."* That is a
   correctness risk hiding inside a success.

### Where that leaves it

Both remaining faults are the same shape and both are ordinary RTL work: give
the memory a registered read and let the consumer take the extra cycle. For
`blit_buf` that means reading beat *n+1* while beat *n* is on the wire, which
the beat counter already makes natural.

I have not done it. It changes protocol timing on two landed blocks and wants
doing carefully rather than at the end of a long session — and the open design
question below may make the `blit_buf` version moot.

**Route to a composed fit, in order:** registered read on those two memories →
synthesis passes → the fitter finally reports real composed ALM/DSP/M10K
numbers → Phase 9 can start. The work-PC handoff stays paused; it is very
likely unnecessary now.

---

## 2026-08-20, later — the buffer is fixed enough to matter: 16.2 GB to 2.7 GB

`CMD.DMA`'s `blit_buf` was `logic [7:0] blit_buf [0:245759]` — a byte array
holding 1.97 megabits. **The byte granularity was never used.** Both sides move
aligned 8-byte groups by construction:

- write: `wr_off <= wr_off + 32'd8` from zero, one `hps_rsp_i.data` word
- read: `wdata_off = b_commit + (wbeat << 3)`, and `b_commit` advances by
  `glen_q`, a 64-byte multiple for a canvas blit

So it was a 64-bit memory described as bytes, and the description cost **245,760
elaboration entries instead of 30,720**. Now `logic [63:0] blit_buf [0:30719]`,
one word in and one word out, no loops.

| | before | after |
|---|---:|---:|
| elaboration peak | **16.2 GB** | **2.70 GB** |

**Verified bit-identical.** Verilator `-Wall` clean, and `cmd_dma_directed`,
`cmd_random`, `cmd_random_soak` all pass — plus the `formal_cmd_dma_crc_gate`
proof at 315 s. Byte *i* of the old read was `blit_buf[wdata_off + i]`; the word
at `wdata_off >> 3` holds exactly those eight bytes in the same order.

**A trap I nearly reported through.** The first test run came back green while
the build had actually FAILED: removing the byte loop renumbered Verilator's
unnamed blocks, the generated files went inconsistent, and ctest ran the *old*
binary. So the green was meaningless. Fixed by deleting the stale model
directory and reconfiguring; the rebuilt binary's hash is recorded before the
numbers above were taken. This is the fourth time this session that a stale
build reported success.

**IT ELABORATES.** `peak = 2.70 GB, 515 s, exit 0`. From "never finishes at
16.2 GB" to done in under nine minutes. The composed shell fit is running now
for the first time since this was found.

**Expect the fitter to fail anyway, and that is fine.** Elaboration succeeding
does not make 1.97 Mbit of flip-flops placeable: the device has roughly 84,000
registers and this asks for about two million. What changes is that the failure
should now be a clear "insufficient resources" from the fitter instead of a
28 GB thrash, and it should come with a real resource report naming the
shortfall. A precise refusal is worth far more than an out-of-memory crash.

**Still not an M10K**, and the two reasons are unchanged: the write lives in an
async-reset process and the read is combinational. An M10K has no reset port and
its read is registered. Fixing that needs a one-cycle read lead in the beat
stream, which is a protocol change, and I have not made it.

### The open design question is unchanged

A **1.97 Mbit on-chip staging buffer** would claim roughly **192 of the device's
553 M10K blocks** — about 35% of all on-chip memory — to hold one canvas so the
CRC can be checked before the first byte is committed.

1. **Two passes over DDR.** Read once to CRC, read again to commit. Costs
   bandwidth, costs zero on-chip memory. The data is already in DDR. *My
   preference.*
2. **Keep it, as real block RAM.** Honest, works, spends a third of the device's
   memory on a staging buffer.
3. **Commit optimistically, invalidate on failure.** Cheapest; breaks the "zero
   guard writes on reject" law in the contract.

Nothing here is decided. It is your call.

---

## 2026-08-20, early morning — one block cannot fit, and it explains everything

### THE FINDING

**`CMD.DMA` cannot be synthesized onto this device as written**, and it is the
reason the composed fit needed 28.4 GB.

```systemverilog
parameter int unsigned BLIT_BUF_BYTES = 245760,
...
logic [7:0] blit_buf [0:BLIT_BUF_BYTES-1] = '{default: 8'h00};
```

That is a **245,760-byte on-chip buffer** — 1.97 **megabits**, a whole canvas —
declared as a flat flip-flop array. Elaborating that one module alone takes
**16.2 GB and does not finish in seven minutes**, while `SDRAM.CTRL` and
`VIDEO.MODE` each finish in **0.26 GB**.

As flip-flops it needs about two million registers. The device has roughly
**eighty-four thousand**. It is not expensive, it is impossible.

It cannot become block RAM either, as written, because it has **both**
disqualifiers at once:

- written inside `always_ff @(posedge clk or negedge rst_n)` (line 380) — an
  M10K has no reset port
- read **combinationally** at line 361 — an M10K read is registered

The block's own comment shows this was known and deferred: *"BLIT_BUF: the
largest canvas, Duo 245,760 B — the M10K mapping is the synthesis lane."* This
is the first time synthesis ever saw it.

**Why the buffer exists:** the blit engine commits a CRC-verified pixel arena,
and the contract promises rejection *before the first byte* reaches VRAM. You
cannot both stream and guarantee that, so it stores the whole canvas first. The
reason is sound; the implementation is not affordable.

**Options, none chosen — this is a design decision, not a bug fix:**

1. **Two passes over DDR.** Read once to compute the CRC, read again to commit.
   Costs bandwidth, costs *zero* on-chip memory. The data is already in DDR.
2. **Restructure as a real RAM** — 64-bit words with byte enables, clock-only
   process, registered read. Still claims roughly 192 of the device's 553 M10K
   blocks, about 35% of all on-chip memory, for one staging buffer.
3. **Commit optimistically, invalidate on CRC failure.** Cheapest, and it breaks
   the "zero guard writes on reject" law in the contract.

I would take (1). It is the only one that does not spend a third of the device's
memory on a buffer.

### THREE OF MY OWN HYPOTHESES WERE WRONG FIRST

Recorded because the wrong ones cost real time:

| hypothesis | measured |
|---|---|
| The shared packages are expensive to parse | 0.24 GB — free |
| The 22-file source cone is expensive to parse | 0.24 GB — free |
| The `-to *` virtual-pin wildcard is the cost | still 16.38 GB with it fixed |

Our own tooling had claimed for months that a leaf block's "~4.8 GB peak is
spent PARSING the 22-file source cone". **That was false**, and believing it is
part of why the composed fit got handed to your work PC. Corrected in
`tools/quartus/run_block_fit.ps1`.

### EVIDENCE I DESTROYED AND RECOVERED

`run_block_fit.ps1` used to write a report containing **only** the modules of
the current run, so any targeted run wiped everything else. It silently
destroyed the original 18-block shell sweep — including the record that
**`zhao_cmd_dma` already failed at map**, which would have found the above
weeks earlier.

Recovered from git and merged back. The script now merges instead of
overwriting and prints its own arithmetic. **42 blocks on record, 35 measured.**

---

## Where the hardware stands

| | |
|---|---|
| Phases 4–5 | complete in RTL |
| Phase 6 | 10 of 11 |
| Phase 7 | 2 of 4, two refused with reasons |
| Phase 8 | 2 of 3, HISTOGRAM refused with reasons, seam composed |
| Phases 9–11 | **not started** |
| Board | **never probed. Nothing has run on a programmed device.** |

**Capacity, 35 blocks measured against `5CSEBA6U23I7`:**

| resource | used | device | |
|---|---:|---:|---|
| ALMs | 29,526 | 41,910 | 70% |
| **DSP** | **180** | **112** | **161% — over** |
| M10K | 40 | 553 | 7% |

DSP is the binding constraint and now has a budget document
(`design/budgets/dsp.md`). ALMs are padded by ~9,000 virtual pins that vanish on
composition. M10K at 7% is headroom worth spending — moving 8,192 bits out of
flops in `TEXTURE.CACHE` recovered 4,315 ALMs.

Seven blocks still unmeasured: `cmd_dma` (fails), `surface_sheet` (fails in the
fitter), and five timeouts.

---

## Do I need to do anything?

**Right now: no.** Nothing is blocked on you.

The composed-fit handoff for your work PC (`reports/composed/README.md`) is
**worth pausing**. It would hit the same `CMD.DMA` wall, just with more RAM to
thrash in. Better to decide the `blit_buf` question first — then the composed
fit may well run on the development machine and you never need the second one.

If you do want to run it anyway, everything still works and the script now
defaults to `-Processors 1`.

---

## Elsewhere

- **Ten planetside suns are live** on zhaozhou.pages.dev, built the way the
  donor actually builds them: sky and sun share one six-bit intensity plane, so
  the largest sun in the set produces the *cheapest* loop.
- **`zhaozhou-site` is now a private GitHub repo**, so the render gallery and
  its append-only diary are under version control.
- **Sabina's sheets** are in `untitled-game/docs/`:
  `konzeptzeichnungen-anleitung.html` and
  `vereinbarung-konzeptzeichnungen.html`. Both A4, Swiss German, no eszett.
- Sorry about the printer. Free RAM hit **0.6 GB** during the Quartus runs,
  which is almost certainly why the print dialog would not open.
