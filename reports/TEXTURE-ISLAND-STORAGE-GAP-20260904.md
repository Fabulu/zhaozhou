# THERE IS NO STORAGE GAP — the brief counts MEMORIES, not bits

**2026-09-04, third and final revision.** This file has been wrong twice today
in the same direction, and the correction is worth more than the original
finding. The whole premise — that "8-10 M10K" and "14-20 M10K" are *capacity*
figures that the RTL misses by 10x and 28x — **was a misreading**.

## What the brief actually says

`reports/islandrearchitecture5.md` §C3, in as many words:

> **CACHE V2 STORAGE: four static data banks + four static tag banks**

Four plus four is **eight memories**. That is the "8-10 M10Ks" of §10.11 — a
count of the distinct RAMs the design is required to have, not a number of bits
it must hold. An M10K holding 1,024 bits is still an M10K.

The same reading fits FRAGROB exactly. §6.1: *"The production FRAGROB owns:
**sixteen fragment slots**"* — so `DEPTH = 16` is not a placeholder, it is the
brief's own number. And its payload arrays are:

    desc_u_m[3][16]  desc_v_m[3][16]  desc_met_m[3][16]
    res_rgb_m[3][16] res_a_m[3][16]   ctx_m[16] auxrgb_m[16] auxa_m[16]

Five per-sample arrays across three samples, plus three per-slot arrays —
**fifteen to eighteen memories**, against a gate of "14-20 M10Ks expected".

**The numbers agree. They always did.** Nothing is 28x too small.

## MEASURED: fragrob's first-ever fit confirms the memories-not-bits reading

`zhao_texture_fragrob` had never been fitted. It has now:

    status           failed:structure    <- ONE violated rule
    registers        2631                <- vs 2500, the only failure
    ramBlocks        13                  <- the brief expected 14-20
    blockMemoryBits  6464
    alms             1676
    dspBlocks        0
    fmaxMhz          103.1

**Thirteen M10K.** The brief's §3.3 budget said *14-20 M10K expected*, and the
tripwire asked for at least 6. **Both are satisfied** — `min_m10k: 6` passes
comfortably, and 13 sits at the edge of the expected band.

**So the "28x storage gap" is finally dead in measurement as well as in
argument.** 6,464 block-memory bits across 13 M10Ks is exactly the shape the
corrected reading predicted: **a dozen-odd small, mostly-empty memories**, which
is what a brief counting RAMs rather than capacity was always describing.

### And one worry did NOT materialise

The corrected entry warned that `desc_u_m[3][DEPTH]` is **multidimensional**,
and that Quartus reports *"cannot regroup multidimensional array"* — the blocker
measured on the texture cache. **It did not bite here.** Thirteen arrays
inferred. The blocker is real for some shapes and was not the shape fragrob has.

### The one real failure, and the rule's message is misleading

    RULE  registers 2631 > allowed 2500
          -- state that belongs in memories is in flip-flops

That message is the rule's canned text, and **here it is wrong**: with 13 M10Ks
holding the payload, the state that belongs in memories **is** in memories. The
overrun is 131 registers, **5%**, and it is control and pipeline state rather
than the entry table the message imagines.

**A gate that fires with a diagnosis attached is more useful than one that does
not — until the diagnosis is wrong**, and then it sends the next person to
re-shape arrays that are already RAM. Worth rewording to state the measurement
(`registers over ceiling`) and let the reader look at `ramBlocks` next to it.

## Two claims retracted

1. **"The blocks are built ~10-28x smaller than the brief."** No. The brief
   never stated a size in bits; this file converted M10K counts into bits by
   multiplying by 10,240 and then treated its own arithmetic as the brief's
   claim.
2. **"One 16x16 RGB565 tile is 512 B, so a lane cannot hold one tile."** That
   tile does not exist in this design — the cache is direct-mapped over flat
   32-bit byte addresses and the brief never mentions tiles. The number was
   imported from general knowledge of how texture caches are usually built.

Both are the same error and it is the one `CLAUDE.md` opens with: *a number
feels like evidence, so it stops getting questioned.* The second was worse,
because it was never measured at all.

## AND IT FLIPS A CAVEAT RECORDED EARLIER TODAY

The run log says *"fragrob's `min_m10k: 6` failure will NOT be an RTL defect"*,
on the reasoning that a 16-deep array cannot fill an M10K so expecting six was
unreasonable. **On this reading it is exactly backwards.**

§6.13's hard rejection list includes:

> any sample/context payload array **in flops** above the explicit control bits

The brief *requires* those arrays to be RAM. It expects 14-20 M10K because it
expects fifteen-odd small memories, each inferred, each mostly empty — and it
**rejects** the implementation where they sit in registers.

And there is a known reason they might not infer: `desc_u_m[3][DEPTH]` is
**multidimensional**, and Quartus reports *"cannot regroup multidimensional
array"* — the exact blocker this session already measured on the texture cache.
So a low M10K count from `fragrob` would be evidence of the defect §6.13 names,
not evidence of an unreasonable gate.

**`min_m10k: 6` on `fragrob` should therefore be treated as a REAL gate**, and
if the fit comes back under it, the fix is to give each payload array a shape
Quartus can infer — not to lower the number.

## The cache tripwire is on the WRONG BLOCK

§10.1 is explicit:

> **Replace, do not patch.** Create `zhao_texture_cache_v2`. Keep
> `zhao_texture_cache` and `zhao_texture_cache_pipe` as **behavioral oracles**
> for line identity, fill order, counters, invalidation and same-line multicast.

`zhao_texture_cache_pipe` is **not the shipping block**. §10.11's gate — 900 ALM,
900 registers, 8-10 M10K, 125 MHz — belongs to `zhao_texture_cache_v2`, which
**does not exist yet**. Applying it to `cache_pipe` is a category error, and
that is why `min_m10k: 8` "failed" against a block that was never going to meet
it.

So today's cache_pipe fit (ALM 1,633, M10K 6) is a measurement of an oracle
against its successor's gate. The measurement is fine; the gate is misfiled.

## What to actually do

* **`cache_pipe`: remove the §10.11 gates from it** and attach them to
  `zhao_texture_cache_v2` when that block is written. Keep fitting cache_pipe
  without them — it is still worth knowing what the oracle costs.
* **`fragrob`: keep `min_m10k: 6`.** It is a real gate for a real requirement,
  and a failure points at the multidimensional-array inference blocker.
* **Nothing needs resizing.** `LINES = 16` and `DEPTH = 16` are not placeholders;
  `DEPTH = 16` is quoted from the brief.
* **`min_memory_bits` stays** — bits are still the sound way to state a capacity
  floor. What was unsound was using it to audit a figure that was never a
  capacity claim.

---

<details>
<summary>The superseded original, kept because the reasoning is the lesson</summary>

# The island's two storage blocks are built ~10–28x smaller than the brief

**2026-09-04.** Found while giving the texture island fits gates that mean
something. It is one finding, not two tripwire arguments, and it is the reason
`min_m10k` has failed three times today.

## The numbers

`reports/islandrearchitecture5.md` states a resource gate per block. Two of them
name M10K counts:

| block | brief §  | brief M10K | that is | block DECLARES | ratio |
|---|---|---|---|---|---|
| `zhao_texture_cache_pipe` | §10.11 | **8–10** | 81,920–102,400 bits | **8,192** | **10x** |
| `zhao_texture_fragrob` | §6.13 | **14–20** | 143,360–204,800 bits | **5,184** | **28x** |

The declared figures are not estimates:

* **cache**: `data_r` is `LANES 4 x LINES 16 x HW_PL 8 x 16 bits` = 8,192.
  `tag_r` adds 1,536 that cannot be memory — 16 deep.
* **fragrob**: `desc_u_m`/`desc_v_m` `[3][16]x32` = 3,072, `desc_met_m`
  `[3][16]x12` = 576, `res_rgb_m` `[3][16]x24` = 1,152, `res_a_m` `[3][16]x8` =
  384. Total 5,184, every array **16 deep**.

**A 16-deep array is not going into an M10K under any circumstances.** So these
blocks cannot approach their briefs' M10K figures — not because of how they are
written, but because of how much they hold.

## Why this matters more than the tripwires

Three `min_m10k` gates failed today and I treated each as its own puzzle:

    zhao_texture_cache_pipe    asked 8,  got 6
    zhao_terrain_residency_v2  asked 17, got 16
    zhao_texture_fragrob       asks 6 from a block with half an M10K of data

The first and third are not tripwire bugs. **They are the brief and the RTL
disagreeing about how big these blocks are**, and the tripwire is simply where
the disagreement surfaced. Lowering the numbers would have hidden it — which is
what I nearly did, and what `min_memory_bits` was added to avoid.

## The two possibilities, and nothing in the tree decides between them

1. **The blocks are placeholders.** `LINES = 16` and `DEPTH = 16` are
   parameters with small defaults; the real cache and the real transaction
   centre hold far more in flight, and the briefs describe the intended sizes.
   If so **these blocks are not finished**, their fits measure a toy, and the
   ALM/register numbers are not the console's.
2. **The briefs counted something else.** Perhaps the texture payload itself
   rather than the descriptors, or a bank-per-lane scheme that was later
   collapsed. If so the briefs' M10K lines are stale and the blocks are done.

**This is an owner question.** It decides whether two of the island's blocks are
finished or barely started, and no amount of fitting answers it — a fit measures
what is written, and what is written is what is in doubt.

## The sizes are PARAMETERS — and one argument here was retracted

Both are parameters with small defaults, which the first draft suspected and did
not check:

    zhao_texture_cache_pipe:  LANES = 4, LINES = 16, LINE_BYTES = 16
    zhao_texture_fragrob:     DEPTH = 16

So possibility 1 — *the blocks are placeholders* — is structurally available,
and at the brief's figures the parameters land somewhere ordinary:

| block | to reach the brief | parameter | result |
|---|---|---|---|
| `cache_pipe` | 8 M10K (81,920 b) | `LINES` 16 -> **160** | 10 KB total, 2.5 KB/lane |
| `cache_pipe` | 10 M10K (102,400 b) | `LINES` 16 -> **200** | 12.8 KB total |
| `fragrob` | 14 M10K (143,360 b) | `DEPTH` 16 -> **442** | 442 in flight |
| `fragrob` | 20 M10K (204,800 b) | `DEPTH` 16 -> **632** | 632 in flight |

### RETRACTED, within the hour, and it is the art law in a new costume

An earlier version of this section said the question was *"answered by one size
comparison"*: a lane holds 256 B, **"one 16x16 RGB565 texture tile is 512 B"**,
therefore a lane cannot hold one tile, therefore placeholder.

**That number was imported, not measured.** `reports/islandrearchitecture5.md`
never mentions a 16x16 tile, and neither does the RTL: `TAG_W = 32 - OFF_W -
IDX_W` makes this a **direct-mapped cache over flat 32-bit byte addresses**.
There is no tile concept in the block at all. The figure came from general
knowledge of how texture caches are usually built, and it *felt* like evidence
because it was a number.

That is exactly the failure `CLAUDE.md` opens with — *"a measured number feels
like evidence, so it stops getting questioned"* — with the aggravation that this
one was never measured in the first place. It survived long enough to be
committed as a recommendation.

### What can honestly be said about the cache's size

* it is **direct-mapped**, `IDX = addr[7:4]`, so addresses **256 B apart alias
  to the same line**;
* per lane it holds **16 lines of 16 B**;
* whether that thrashes depends on the **access pattern the brief specifies**
  — bilinear taps, row strides, whether the texture layout is linear or
  swizzled — and **this file has not read that part of the brief**.

So the sizing question is **not** settled. What IS established is that the gap is
reachable by a parameter, which was the actual open point: the brief and the RTL
are not describing incompatible designs, they are describing the same design at
two settings.

### What this still changes about the fits

**The island fit numbers are numbers for `LINES = 16` and `DEPTH = 16`**, whatever
the right settings turn out to be. Raising `LINES` tenfold moves the block from
MLAB-and-registers into M10K — a different implementation with different ALM,
timing and routing, not the same block with more storage. Read the island fits
as measurements of the committed parameters, which is all any fit ever is.

### The recommendation, narrowed to what is supported

* **Do not raise either parameter to make a tripwire pass.** That is the move
  `min_memory_bits` was added to prevent.
* **`cache_pipe`: read the brief's access pattern, then decide.** The retraction
  above is what that costs when skipped.
* **`fragrob`: ask.** 442-632 in flight is a lot to infer from a brief, and the
  brief may have counted the returned texel payload rather than the descriptors.

## What was done, and what deliberately was not

* `min_memory_bits` added to the rule vocabulary — a sound floor, since bits do
  not depend on how Quartus packs them.
* `cache_pipe` gets `min_memory_bits: 8192`, derived from `data_r` alone.
* **`fragrob` gets no bits floor.** Every array is 16 deep, so the honest
  expectation is *zero* block memory bits, and a floor that passes by
  construction would be theatre.
* **No brief number was changed**, and no tripwire was relaxed to match a
  measurement. `min_m10k: 8` and `min_m10k: 6` stay exactly as the briefs set
  them, because they are the evidence that this question exists.

</details>
