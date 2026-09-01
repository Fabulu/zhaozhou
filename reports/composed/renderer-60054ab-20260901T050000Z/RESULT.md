# Composed fit — constant-multiply columns — 2026-09-01 (round 7) — REVERTED

**A clear regression, and the check written into the previous commit is what
caught the cause.**

    gpu_clk       84.97 -> 66.78 MHz    -18.19
    worst setup   -1.769 -> -4.975 ns
    endpoints     808 -> 1,723
    DSPs          16 -> 28              <-- the cause
    ALMs          12,698 -> 12,616

## What happened

Round 6's worst path was `sx1_r -> Add106 (two levels) -> Add112 -> fill test`.
Since `gi` is a genvar, `gi * sx` is a **constant** multiply whose minimal form
is usually one operation — `15*sx = (sx << 4) - sx` for the critical column. So
the four-term tree was replaced with the multiply, expecting the synthesiser to
pick the canonical signed-digit form.

**It inferred DSP blocks instead. Twelve of them.**

A DSP costs ~3.785 ns on this device (measured on the round-3 path); a LUT adder
level costs ~0.5. Trading two adder levels for a DSP multiply made the path far
worse, and it also pulled the products off into DSP columns, which is why the
endpoint count more than doubled.

## The check was written down in advance, and that is the point

The commit that made this change said:

> The fit will check the other thing worth checking: that a constant multiply
> does not infer a DSP. It should not; **"should not" is not a measurement**,
> and the count has been 16 for seven fits.

It was not. **DSP count is now a standing check on this design** — it had been
16 across seven consecutive fits, so any movement is a structural change worth
explaining before the Fmax is even read.

## Reverted

Back to round 4's balanced two-level tree, which is the 84.97 MHz state.
Verified: edgewalk directed 146, tile_pipe directed 74.

## What this says about the remaining 1.769 ns

The idea was not wrong — `(sx << 4) - sx` really is one operation, and it really
would shorten the path. **The mistake was expressing it in a form the tool
reads as a multiplier.** If this is retried it must be written as an EXPLICIT
shift-and-subtract per column, generate-cased on `gi`, so no multiplier can be
inferred:

    gi == 15  ->  (sx <<< 4) - sx
    gi == 14  ->  (sx <<< 4) - (sx <<< 1)
    gi ==  7  ->  (sx <<< 3) - sx
    ...

That is more verbose and it is the version that cannot be misread. Whether it is
worth doing before the Early-Z and streamed-row steps is a separate question —
those are `MHZArchitected` steps 2a and 3, and both target blocks currently in
the worst 100.
