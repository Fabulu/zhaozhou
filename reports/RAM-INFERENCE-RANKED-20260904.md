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

## What to do next, in order

1. **`zhao_terrain_residency_v2`**: reasons 1 and 2, using the
   `zhao_texture_cache_pipe` pattern already in the tree. Then fit it against
   its own tripwires — `min_m10k` is the important half, as always.
2. **Reason 3** needs a decision about the sweep, not an edit. It is the only
   item here that is a design question.
3. `zhao_raster_tilestore` (32,768) and `zhao_texture_palette_res` (16,384) are
   both single-reason (async reset only) and should be mechanical.
4. Re-run `--rank` after each and put the delta in the run log. The tool is
   cheap enough to run every time; the fit is not.
