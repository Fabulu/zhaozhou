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
