# Packet 1's two falsifiers: the laboratory is NOT load-bearing

2026-09-08. The mutation is a swap of `u_metajoin`'s `.wr_frac_u_i`/`.wr_frac_v_i`
on the write side — the metadata bank stores each sample's U fraction where its V
fraction belongs. Applied, run in both profiles, reverted via `git checkout`
(the file is tracked, unlike the trap hit earlier today with an uncommitted tb).

## Falsifier 1 — lab profile: does the laboratory still detect?

Required: the shadow mismatch counter must fire and the composed test must fail.

```
metajoin shadow: 1176 comparisons, 192 mismatches
metajoin per-queue: bil 768 checked/768 wrong, near 192 checked/0 wrong
FAIL  ... returned EXACTLY what sampmeta_m, palslot_m and palgen_m would return
      expected 0, got 192
```

**It fires.** 192 shadow mismatches and every one of the 768 bilinear queue
comparisons wrong. The apparatus that `MIGRATION_SHADOWS=1` claims to carry is
carried and works.

The `near` queue reads 192 checked / **0 wrong**, and that is correct rather than
a miss: a nearest sample uses no fractions, so swapping them cannot change what
it retires. A detector that fired there would be reporting a difference that does
not exist.

## Falsifier 2 — production profile: does anything catch it without the lab?

This is the one that could have stopped the packet. §4.3 draws a boundary
between migration proof and correctness enforcement, and the boundary is in the
wrong place if the laboratory was quietly doing the enforcing.

```
production profile: shadow comparators not elaborated
FAIL  every BILINEAR fragment retires EXACTLY what zref::Tmu::sample computes
      ... expected 0, got 32
FAIL  every ARGB4444/bilinear fragment retires EXACTLY the colour ...
      expected 0, got 32
FAIL  and every ARGB4444/bilinear fragment's ALPHA matches the reference ...
      expected 0, got 29
[island_composed_directed] 3/124 checks FAILED
```

**Caught, by the reference differential, with no shadow present.** The same three
colour checks that fail in the lab profile fail in production — they compare
retired fragments against `zref::Tmu::sample`, which has no idea the laboratory
exists.

So the laboratory is **apparatus, not enforcement.** Removing it costs migration
*evidence* and costs nothing in defect *detection*, which is exactly the claim
§4.2 needs and the reason Packet 1 may proceed to its fit.

## What this does not show

It does not show the laboratory is useless. The shadow is far more *specific*: it
names 192 wrong metadata records and localises them to the bilinear queue, while
the colour differential says only "32 fragments came out the wrong colour". On
2026-09-08 that difference is what turned a bilinear failure into a diagnosis in
one run instead of a bisect. Keeping the lab profile is worth its parameter.

Nor does it show what the laboratory *costs*. That is FIT GATE 1 — a controlled
pair, same functional source, `MIGRATION_SHADOWS` the only difference.

## Recorded state after the run

Mutation reverted, `git status -- fpga/rtl` clean, zero `MUTANT` markers, and
both profiles green again at 125 (lab) and 124 (production).
