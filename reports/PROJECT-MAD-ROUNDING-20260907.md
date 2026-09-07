# `rescale16_mad`'s rounding is unreachable for every EVEN viewport

*2026-09-07. Found while closing a coverage hole, and it turned out to retire
the hole instead.*

---

## The claim

`zhao_project_core` computes

```systemverilog
mad_x    = ndc_x * (vp_w << 15) + (cx13 <<< 32)
scr_fx_x = rescale16_mad(mad_x)          // (x + 32768) >>> 16, round-half-up
```

The `+ 32768` can only change the result when `mad & 0xFFFF == 0x8000`. But
`vp_w << 15` already has **15 trailing zeros**, so one more factor of two puts
it past 16:

| viewport dimension | trailing zeros in `vp << 15` | half reachable |
|---|---:|---|
| 256 | 23 | **no** |
| 192 | 21 | **no** |
| 320 | 21 | **no** |
| 200 | 18 | **no** |
| 2, 6 | 16 | **no** |
| 1, 3, 5, 7 | 15 | yes |

**For any even viewport width or height, `rescale16_mad` is an exact shift and
its rounding term is dead.** Only an odd dimension can reach it.

The `cx13 <<< 32` addend cannot rescue it either — it has 32 trailing zeros, so
it never touches bit 15.

## What this settles

**`geom_project_directed` is not blind after all.** It was recorded earlier
today as a known coverage hole because a mutation to `rescale16_mad` left its
900 checks passing. It uses `view_of(0,0,256,192)`, `view_of(0,192,256,192)` and
`view_of(7,3,320,200)` — **every dimension even** — so the rounding it "fails to
test" is unreachable in every configuration it exercises. That note is
withdrawn: the suite is correct, and adding an odd-viewport case there would be
testing a configuration it deliberately does not use.

**`terrain_project_directed`'s case 7 is the one that matters**, and it uses
`{1,1,3,5}` — odd on both axes, chosen to stress the arithmetic. It now tests
the rounding *observably* rather than merely reaching it.

## The question this raises, which is not a test question

If the console only ever configures even viewport dimensions, then
`rescale16_mad`'s round-half-up **never fires in the shipped machine**. It is
not wrong — the RTL accepts odd viewports and the reference specifies
round-half-up for them — but it is worth knowing that a ratified rounding rule
is, in practice, exercised only by test configurations.

Two things follow, and neither is decided here:

* **If odd viewports are legal**, the rounding is live and the terrain suite is
  now the only thing testing it. That is thin but sufficient.
* **If odd viewports are not legal**, then this is dead logic on a path that
  §PROJECT-CORE-CLOCK-20260907 measures at 61.09 MHz, and the saturation and
  rounding around `scr_fx` could be simplified — which is an owner ruling about
  the viewport contract, not an implementation choice.

## Method note

This was reached by trying to close a hole, computing the reachability
condition, and finding the condition could not be met. **The negative result
was the finding.** A search that comes back empty is evidence when the space it
searched is stated: every viewport 1..32 against every integer divisor 1..16,
then the trailing-zero argument that explains why.
