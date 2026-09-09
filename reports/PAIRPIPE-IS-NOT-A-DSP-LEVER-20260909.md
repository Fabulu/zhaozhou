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
