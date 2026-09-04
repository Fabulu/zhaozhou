# The console does not fit, and one file is 60% of the reason

**2026-09-04.** Produced by `tools/quartus/check_ram_inference.py --rank`,
filtered through `design/prod_manifest.yml`. Both are committed; every number
here is reproducible with two commands.

---

## The number

**280,784 bits of declared array in the production top will not infer as
memory.** Each of those bits therefore needs a flip-flop.

Two bounds on what that costs, both taken from our own measurements rather than
from a datasheet claim:

| bound | reg/ALM | ALMs needed | device has |
|---|---|---|---|
| Cyclone V best case, fully packed | 4.00 | **70,196** | 41,910 |
| measured in our own shell fit (12,707 ALM held 14,812 reg) | 1.17 | **240,881** | 41,910 |

The generous bound is **1.7x the device**. The bound measured on this design is
**5.7x**. The shell fit that produced the 1.17 figure was 45 source files — a
fraction of the console — and it already spent 12,707 ALMs.

This is not a tuning problem. It is the answer to "what does the planned console
cost when counted once", and the answer is that it does not fit.

## Where it is

    IN THE PRODUCTION TOP                              234,199 bits
      168,876   terrain/zhao_terrain_residency_v2.sv    <-- 72% of the total
       16,384   texture/zhao_texture_palette_res.sv
       12,288   forge/zhao_forge_cliff.sv
        6,400   raster/zhao_raster_texjoin_v2.sv
        5,504   texture/zhao_texture_tmu_pipe.sv
        ...     20 more, none above 2,700

    LEAVES INSIDE COUNTED TOPS                          46,585 bits
       32,768   raster/zhao_raster_tilestore.sv
        5,184   texture/zhao_texture_fragrob.sv
        5,049   raster/zhao_raster_toon_div.sv
        ...

    EXCLUDED, and correctly so                         195,728 bits
      139,776   synth/zhao_probe_patch_acc.sv           [a fit probe]
       31,744   terrain/zhao_terrain_residency.sv       [superseded by v2]
        ...     8 more probes and superseded blocks

**The exclusion column is the point of doing this properly.** A source
inventory would have reported 476,512 bits and sent the next pass at
`zhao_probe_patch_acc` — a synthesis probe that the console never
instantiates, and the second-largest entry in the unfiltered list.

## The single finding

`zhao_terrain_residency_v2.sv` holds **167,936 bits in two arrays**:

    logic [KEYW-1:0]  keyram  [WAYS][SETS];   // 107 bits x 1024 = 109,568
    logic [STATW-1:0] statram [WAYS][SETS];   //  57 bits x 1024 =  58,368

This is a 4-way, 256-set residency cache. It is exactly the shape an M10K
exists for, and it will not become one, for **three** reasons the checker names
separately — and all three have to be fixed, because any one of them alone is
sufficient to keep the whole array in flops:

1. **Written from an `always_ff @(posedge clk or negedge rst_n)`** at 7 sites.
   An M10K has no reset port. This is the same defect that cost an 85-minute
   fit yesterday on the texture cache, and it was already documented in a
   comment inside the block that cache replaced.
2. **`[WAYS][SETS]` is multidimensional.** Quartus says so in as many words —
   *"EDA Netlist Writer cannot regroup multidimensional array"* — and emits no
   "Inferred RAM" line at all. It muxes across the outer dimension and the
   whole array falls into flip-flops however correct the writes are. The fix is
   one flat array per way inside a `generate`, outer index a genvar.
3. **Three distinct dynamic write addresses** (`[ev_way_c][s0_set]`,
   `[victim_c][s0_set]`, `[w][sweep_q]`). The island brief's S5.3 forbids this
   by name: it asks for a memory shape the device does not have.

Fixing 1 and 2 is mechanical and was already done once this session, on
`zhao_texture_cache_pipe.sv` — the pattern exists and is committed. **Fixing 3
is a design change**, because the sweep walks ways independently of the lookup
path, and that is a scheduling question rather than a syntax one.

At 167,936 bits this one file is **60% of the whole production overage**. If it
became memory it would cost roughly 17 M10K out of 553.

## What this is not

**Declared bits are not synthesised flops.** An array that is written but never
meaningfully read gets folded away; a small one packs into ALM registers more
efficiently than the 1.17 ratio suggests. The checker reports *a reason an array
will not become an M10K*, which is not the same as *proof it consumes flops*.

So treat the totals as an upper bound with the right shape, not as a fitter
result. **Only the fitter knows what the tool did.** What the ranking is for is
deciding where to spend the next fit — and on that it is unambiguous: one file
is 60% of the problem and the next four together are under 17%.

## The ranking itself was wrong twice before it was right

Worth recording, because both failures are the house error in miniature.

**First run: 898 findings, unranked.** True and unusable. A two-entry `state
[0:1]` that is *correctly* in flops sat in the list looking exactly as important
as a 109,568-bit cache store. A finding list with no magnitude is a list nobody
acts on.

**Second run: "12 arrays worth looking at", and 331 UNKNOWN.** The ranking
printed a confident shortlist while every parameterised store in the console —
including all five of the largest — sat unresolved in a pile below it. Two
separate bugs, both from a heredoc eating backslashes:

* `\b` in the parameter regex became a literal `0x08`, so `local_params`
  matched **nothing** and only fully-numeric declarations ever sized;
* `[^;,]+` had no newline in its terminator set, so a module parameter — which
  has no terminator of its own — swallowed the entire port list after it.

The tell was not a crash. It was **331 UNKNOWN against 12 ranked**, a ratio that
does not happen when a sizer works. The lesson is the one already in CLAUDE.md
wearing new clothes: a measurement that *feels* like evidence stops getting
questioned. A ranked list is far more persuasive than an unranked one, and this
one was ranking almost nothing.

The fix is committed with the tool, and the file is now asserted free of control
characters so the same corruption cannot return silently.

## RESULT, same night

All three mechanical items done, each verified against a baseline captured
before the edit and each proved to bite by a mutation.

| | before | after |
|---|---|---|
| `terrain/zhao_terrain_residency_v2` | 168,876 | **940** |
| `raster/zhao_raster_tilestore` | 32,768 | **0** |
| `texture/zhao_texture_palette_res` | 16,384 | **0** |
| **production total** | **280,784** | **63,696** |

**A 77% reduction, with byte-identical behaviour.** The bound that mattered:

| bound | before | after | device |
|---|---|---|---|
| best case, 4 reg/ALM | 70,196 ALM | **15,924 ALM** | 41,910 |
| this design's measured 1.17 | 240,881 ALM | 54,441 ALM | 41,910 |

The generous bound now fits with room. The pessimistic one no longer applies to
the fixed arrays at all — the point of the change is that those 216,704 bits
become roughly 22 M10K out of 553 rather than ALM registers of any density.

### The third reason was not a design question

The report above said `zhao_terrain_residency_v2`'s three distinct write
addresses needed "a design change, because the sweep walks ways independently
of the lookup path". **That was wrong, and the mistake is instructive.**

`[w][sweep_q]`, `[victim_c][s0_set]` and `[ev_way_c][s0_set]` are mutually
exclusive **per way**: the sweep is the `if` arm and the pipeline the `else`; a
claim and an event are different arms of one `unique case`; and a claim and an
event write the *same* address. Per way there is one address and one enable.

The `[WAYS][SETS]` shape was hiding the per-way view, so **one defect was
producing two findings** and the second one looked like a wall. Fixing the shape
dissolved it. Worth remembering the next time the checker reports "a memory
shape the device does not have": check whether the shape is the reason.

### What the conversion cost

The write decision moved into an `always_comb` producing
`{address, per-way enable, data}`. It has to be combinational — registering the
intent would add a clock and change every latency the tests pin — so it is a
**second copy of the arm conditions**, and that is a real maintenance cost paid
deliberately against 167,936 bits.

It is guarded rather than trusted: changing the claim's `kwe_c[victim_c]` to
`kwe_c[0]`, exactly the drift this duplication risks, fails 7 of 37 directed
checks and 3 of 6 random ones.

### One property became load-bearing

`hazard_c` blocks an access whose `addr_c` equals `s0_set` for exactly the
events that write. In flip-flops that decided nothing — a non-blocking read is
always the old value. **In an M10K, mixed-port read-during-write is device
behaviour**, so an existing guard that used to be belt-and-braces is now what
makes the conversion sound. It is recorded as such in the RTL, because the next
person to relax it would have no way to know.

## What to do next, in order

1. **Fit `zhao_terrain_residency_v2`.** This is the only thing that closes the
   claim. `min_m10k` is the acceptance criterion and the important half — a fit
   that reports 0 M10K has failed however good its Fmax is. Expect roughly 22.
2. `forge/zhao_forge_cliff` (12,288) is now the largest remaining, and it is
   single-reason: async reset, plus a module-scope read through a dynamic index
   that has to move into the port process with it.
3. `raster/zhao_raster_texjoin_v2` (6,400) and `texture/zhao_texture_tmu_pipe`
   (5,504) are the C6–C8 banking work already scheduled.
4. Everything below 2,700 bits is control state. Leave it alone unless a fit
   says otherwise: an array that small cannot pay for an M10K however it is
   written, and rewriting it would trade clarity for nothing.
5. Re-run `--rank` after each and put the delta in the run log. The tool is
   cheap enough to run every time; the fit is not.


---

# CORRECTION — TWO OF THE THREE BLOCKS WERE ALREADY INFERRING

**2026-09-04, found by checking the recorded fit rows against the claim.** The
"280,784 bits will not infer" headline below **over-counted by at least 49,152
bits**, and the "77% reduction" figure counted bits that were never in
flip-flops.

`reports/synthesis/zhao_block_fit.json` already held these rows, from before any
change tonight:

| block | commit | date | M10K | block memory bits |
|---|---|---|---|---|
| `zhao_texture_palette_res` | `d656521` | 2026-09-02 | **2** | **16,384** |
| `zhao_raster_tilestore` | `96c0394a` | 2026-08-20 | **4** | **32,768** |
| `zhao_texture_cache_pipe` | `8faaa240` | 2026-09-03 | 2 | **128** |

**16,384 is exactly `mem_r`. 32,768 is exactly `ram0` + `ram1`.** Both arrays
were already fully in block memory. The `palette_res` row is `clean=True`, so
that one is not in doubt.

## What was actually wrong with the checker

Both blocks were flagged **only** for the async-reset rule — *"an M10K has no
reset port, so this array cannot be one"*. That rule is **too strong**. Quartus
17 will infer a RAM from a process with an asynchronous reset provided the ARRAY
itself is never reset, which is exactly what those two blocks did.

The rule was written from the texture-cache evidence, where inference genuinely
failed — but that block was ALSO multidimensional, and **that** is what blocked
it: 128 bits of block memory, and Quartus said so in as many words ("cannot
regroup multidimensional array"). The async reset was correlated, not causal,
and the checker generalised from one case where two defects appeared together.

## What survives

* **`zhao_texture_cache_pipe` is a real win.** 128 bits of block memory before,
  6 M10K and 3,033 registers after — and the blocker was the MULTIDIMENSIONAL
  shape, not the reset.
* **`zhao_terrain_residency_v2` is a real win.** It had no prior fit row at all,
  and 167,936 bits now sit in 16 M10K at 2,229 ALM.
* The two port moves are still correct — they let the M10K's own output register
  be used — but they were **not** what put those arrays into memory, and this
  report said they were.

## CONFIRMED BY MEASUREMENT

`zhao_raster_tilestore` came back **`ok`, 3,394 s, ALM 859**. `ok` means it
passed its tripwires, `min_m10k: 4` among them, so it has at least 4 M10K —
**the same as it had at `96c0394a` before any change today.**

**The change was a no-op for inference, exactly as the stale row predicted.**
That is now measured rather than deduced.

**But it was not worthless:** ALM fell **929 → 859**, a saving of 70. That is
consistent with what the port move actually does — a read register carrying a
reset cannot be the M10K's own output register, so removing the reset let those
registers move inside the block. The benefit is real and it is 70 ALMs, not
32,768 bits.

So the honest ledger for the three blocks:

| block | claimed this morning | measured |
|---|---|---|
| `zhao_texture_cache_pipe` | bits out of flops | **true** — 128 bits → 6 M10K, registers 11,328 → 3,033 |
| `zhao_terrain_residency_v2` | bits out of flops | **true** — no prior fit; 167,936 bits in 16 M10K |
| `zhao_raster_tilestore` | bits out of flops | **false** — already inferring; −70 ALM |
| `zhao_texture_palette_res` | bits out of flops | **false** — already inferring at 2 M10K |

---

# THE FIT CAME BACK: 6 M10K, 3,033 registers, 1,633 ALM

**2026-09-04, 4,553 s of quartus_fit.** The storage rework is measured, and the
prediction below was right to the block.

| | before | after | |
|---|---|---|---|
| **M10K** | 2 | **6** | +200% |
| **registers** | 11,328 | **3,033** | **−73%** |
| **ALM** | 5,903 | **1,633** | **−72%** |

**Roughly 8,300 registers left flip-flops and became memory.** That is the whole
claim of the rework, and it is now a fitter number rather than a synthesis
promise.

**It still reports `failed:structure`, on all three tripwires:**

    M10K 6 < required 8        -- see the ruling below; this one is EXPECTED
    registers 3033 > 2000      -- a genuine miss
    ALM 1633 > 1500            -- a genuine miss

**The M10K failure is exactly the case documented below and predicted at 4–6.**
It came in at 6. That is not a defect and must not be read as one — the block
has four `data_r` banks plus two small FIFOs worth being memory, and `tag_r` is
16 deep, which no M10K will ever hold. **`min_m10k: 8` is unreachable and the
ruling below is still open.**

**The other two are real and are now small.** 3,033 against 2,000 and 1,633
against 1,500 are misses of 52% and 9%, against the 466% and 294% they were
before. What remains in flip-flops is the block's control state plus `tag_r`'s
1,536 bits, and S3.4's ALM and register ceilings were written for a block whose
storage was assumed to be entirely in memory — the same assumption that produced
`min_m10k: 8`. **All three numbers are probably one question, not three.**

---

# OWNER DECISION: `min_m10k: 8` on TEXTURE.CACHE cannot be met

**Not changed. Recorded for a ruling**, because lowering a gate so the thing
passes is the exact failure the gate exists to prevent, and the number is from
island brief S3.4.

The 23:52 fit was killed before the fitter finished, but **its synthesis
completed and is decisive**:

* `"cannot regroup"` — the message that was the actual blocker last time — is
  **gone**;
* all four lanes' `data_r` became `altsyncram`, Simple Dual Port, 128 x 16,
  **2,048 bits each**;
* total block memory bits **8,320**, against **128** on the previous fit.

The storage fix worked. But `tag_r` is not in that list, and it will never be:

    logic [TAG_W-1:0] tag_r [LINES];   // 16 deep x 24 wide = 384 bits per lane

**384 bits, 16 deep, is below the size Quartus will put in an M10K at all.** It
is the same "uninferred due to inappropriate RAM size" verdict `rq_en` gets in
this very report, and flip-flops are the *correct* answer for an array that
shape — forcing it would burn four M10Ks to hold 1,536 bits.

So the block has **four** arrays worth being memory, not eight:

| array | per lane | x4 lanes | can it be an M10K? |
|---|---|---|---|
| `data_r` | 2,048 bits (128 x 16) | 8,192 | **yes — and it is** |
| `tag_r` | 384 bits (16 x 24) | 1,536 | no, too shallow |

9,728 bits total, which is exactly the "9,728 bits of array in flip-flops"
figure this whole rework started from.

`zhao_prod_top` instantiates the block with **no parameter override**, so
`LANES=4, LINES=16, LINE_BYTES=16` are the shipping numbers, not placeholders
that a bigger production config would replace. Checked, because the entire
argument turns on it.

### The two possible rulings

1. **The tripwire is wrong.** By the same capacity-floor method used for
   tonight's other three blocks, the defensible number is **`min_m10k: 4`** —
   four `data_r` banks of 2,048 bits, each needing its own block. S3.4's 8 was
   presumably written expecting the tag array to be memory too.
2. **The cache is wrong.** If 8 blocks of storage was the architectural intent,
   the block is half the size it was meant to be and `LINES` should grow. That
   is a cache-capacity decision with a hit-rate consequence, not a syntax one.

**Until this is ruled, a failing `min_m10k` on this block is not evidence of an
RTL defect.** That is the thing most likely to be misread from a red fit
summary, which is why it is written down here rather than left in a run log.
