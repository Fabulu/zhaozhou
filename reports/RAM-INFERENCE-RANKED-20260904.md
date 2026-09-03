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
