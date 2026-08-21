# Status — for Fabian

*This file is the channel. I update it and push; you read it here. Newest section
at the top.*

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
across **89** blocks. `ctest -L fast`: **238/238**.

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
block rather than a rewrite. `ctest -L fast`: **238/238**.

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
