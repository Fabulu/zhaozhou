# The pair-pipe is not a DSP lever, and my multiplier counter was broken

2026-09-09. The DSP census addendum listed `zhao_raster_perspuv_pairpipe` with
"DSP: unknown -- **NOTHING, never fitted**", as the one lever with no evidence at
all. It can be settled from structure, with no Quartus, and the answer is no.

## The count

`zhao_raster_perspuv_svc` contains exactly ONE multiply statement:

```systemverilog
for (int unsigned ax = 0; ax < 2; ax++) begin
  ...
  p1_prod_q[ax] <= 64'(p0_num_q[ax]) * $signed({40'd0, p0_mant_q[ax]});
end
```

Two axes, so **two physical multipliers** -- a function call or a loop body is not
shared hardware, which is the lesson the combiner's fourteen `unit_mul` sites
already taught this repository. At 3 DSP for a wide product that is 6, and the
block **measures 6**. The structure and the fit agree exactly.

`zhao_raster_perspuv_pairpipe` contains two:

```systemverilog
p1_pu_q  <= 64'(p0_nu_q) * $signed({40'd0, p0_mant_q});
p1_pv_q  <= 64'(p0_nv_q) * $signed({40'd0, p0_mant_q});
```

**Also two multipliers.** The pair-pipe's saving is one scheduler, one mantissa
register instead of two, and the deletion of the operand tables, the `e_have`
join and the two result tables -- ALM and registers, all of it. **Not one DSP.**

### PRE-REGISTERED PREDICTION

`zhao_raster_perspuv_pairpipe` will map to **6 DSP**, the same as
`perspuv_svc`. If it comes back lower, my reading of "two statements, two
multipliers" is wrong and I want to know that. Recorded before the MapOnly runs,
because a prediction written afterwards is a description.

The honest consequence for the budget: **the pair-pipe comes off the DSP lever
list.** It is still worth doing on ALM, and the DSP census should say so rather
than leaving it as an unexplored hope, which is the more flattering shape.

## And the counter I used to find this was broken

The first pass ran this over both files:

```
grep -nE "[a-zA-Z0-9_)\]]\s*\*\s*[a-zA-Z0-9_($]"
```

and reported **0 multiply sites in both**, which would have meant a block with no
multiplier measuring 6 DSP. That is a mystery, and mysteries are where wrong
conclusions get built.

**It was caught by a positive control**, not by suspicion of the number: running
the same pattern over `zhao_project_core.sv`, which unambiguously contains
`$signed({...}) * $signed({...})`, also returned 0. A pattern that cannot find a
multiply in the file whose entire DSP story is multiplies is not measuring
multiplies.

Note the direction, again. Reporting **zero** multiplier sites makes a design look
like it has nothing to optimise and no discrepancy to explain -- the comfortable
answer. A counter that over-reported would have been fixed in the first minute.

The replacement is not a cleverer regex. These files are 295 and 622 lines, so the
right tool was `grep -n '\*'` and reading the three hits, which took less time
than writing the pattern did. **A hand-checkable file does not need an
instrument**, and CLAUDE.md's rule 3 says to check the heuristic against a case
you can verify by hand before believing the total -- here the hand check WAS the
measurement.

---

# THE PREDICTION SCORES: 6 DSP, exactly as filed

`zhao_raster_perspuv_pairpipe@map` landed. Clean tree, real digest.

| | DSP |
|---|---|
| `zhao_raster_perspuv_svc` | 6 |
| `zhao_raster_perspuv_pairpipe` | **6** |

**Filed before the measurement:** *"`zhao_raster_perspuv_pairpipe` will map to 6
DSP, the same as `perspuv_svc`. If it comes back lower, my reading of 'two
statements, two multipliers' is wrong and I want to know."*

It came back 6. The structural reading holds -- a loop body is not shared
hardware, `svc`'s single multiply inside `for (ax = 0; ax < 2)` is two
multipliers, and the pair-pipe's two multiply statements are also two. **The
pair-pipe stays off the DSP lever list**, confirmed by measurement rather than by
argument.

## AND THE OTHER COLUMNS INVITE A MISMATCHED COMPARISON, so I am not making one

The two rows are not the same kind of measurement:

| | kind | DSP | registers | memory bits |
|---|---|---|---|---|
| `perspuv_svc` | **full fit** | 6 | 3,216 | 256 |
| `perspuv_pairpipe` | **MapOnly** | 6 | 961 | 1,280 |

"2,255 fewer registers" is sitting right there and it would be wrong. Fitting
REPLICATES registers, and the two map/fit pairs this ledger holds show how much:

```
zhao_texture_combine              FIT 524 regs   MAP 304 regs
zhao_texture_material_combine_v1  FIT 1269 regs  MAP 744 regs
```

About 1.7x in both. So the pair-pipe's 961 map registers are perhaps ~1,600
fitted, against `svc`'s 3,216 -- still a large saving, and **still not a number I
have measured.**

**DSP is the one column that IS safe across the two kinds**, and that was checked
rather than assumed: both pairs above report identical DSP in map and fit (8/8
and 2/2). Which is precisely why the prediction was filed about DSP and is
scorable now.

The memory-bits column (1,280 against 256) points the way the pair-pipe's design
intends -- one scheduler and one mantissa register instead of two -- but the only
map/fit pairs available to validate that column are both zero, so it corroborates
nothing yet.

**`zhao_raster_perspuv_svc@map` is running now** to make the register and memory
comparison like-for-like. Minutes, not hours, and it is the difference between a
claim and a measurement.
