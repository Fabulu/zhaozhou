# The projection core's 32-bit operands: a contract property, not an oversight

2026-09-09. Follows the DSP census addendum, which named operand-width narrowing
in `zhao_project_core` as "the cheapest unexplored move in the largest item in the
budget" and said it was "answerable in simulation at zero fit cost". **Both halves
of that sentence need correcting**, and the correction is more useful than the
claim was.

## 1. It is not unexplored, and it is not merely an engineering question

`design/contracts/GEOM.PROJECT.md` states the width choice and its reason:

> "The row accumulator is 68 bits, wider than the widest sum any input word can
> produce (3 x 2^62 + 2^47 < 2^64), so it cannot wrap for ANY input rather than
> merely for legal ones."

Full width is a **declared, reasoned robustness property**: the block is correct
for any s32 the register map can hold, not merely for the matrices a well-behaved
caller writes. The contract declares matrix words 0..15 as `fx16` and **bounds
them nowhere.**

So narrowing the matrix operand is a **contract change**, and it trades a stated
property for DSPs. That is an owner decision, not a cleanup. My addendum framed
it as the latter, which is the "first explanation that absolves the design" law
running in its usual direction -- the reading that made a big win look free
arrived first.

## 2. But the property is CLAIMED more widely than it is TESTED

`tests/geometry/geom_project_directed.cpp:270-274`, the random section:

```cpp
for (int k = 0; k < 16; ++k) r[k] = static_cast<int32_t>(rng.next()) >> 14;
r[12] = 0;
r[13] = 0;
r[14] = static_cast<int32_t>(rng.next()) >> 15;
r[15] = static_cast<int32_t>(rng.next()) >> 14;
```

against the vertices two lines later:

```cpp
v.x = static_cast<int32_t>(rng.next()) >> static_cast<int>(rng.below(14));
```

| operand | random-test range | bits used | of 32 |
|---|---|---|---|
| matrix entry | +-131,072 raw = **+-2.000** in Q16.16 | **19 signed** | 19 |
| `r[14]` (w-row z) | +-65,536 = +-1.000 | 18 signed | 18 |
| `r[12]`, `r[13]` | **constant 0** | 0 | 0 |
| vertex x/y/z | full s32 = +-32,768.0 world units | **32 signed** | 32 |

**The vertices are swept to the full width. The matrix entries are not** -- they
are capped at 19 bits, and two of the nine multiplier operands are held at
literal zero. So on exactly the operand where narrowing would pay, "correct for
ANY input" is asserted by the contract and exercised over 19 of 32 bits.

That is not a defect. The test's own comment shows the cap was reasoned, not
accidental -- *"a pose-plausible matrix: the 3x3 near unit scale, the translation
carrying the range, and a w row that actually varies with z"* -- and `r[12] =
r[13] = 0` is what a real perspective matrix has. It is the right choice for
testing plausible poses. It is simply **not** coverage of the claim the contract
makes.

## 3. What the prize plausibly is, and why I am not going to state it

Cyclone V variable-precision DSP does one 27x27 signed, or two 18x18. The census
infers ~3 DSP per 32x32 from 33 over eleven multipliers. A 19x32 should need
fewer than a 32x32 -- but **"3 becomes 2, so 66 becomes 44" is precisely the
division-of-a-total that the census itself flags as an inference and that this
repository has been burned believing twice this week.** Two magnitude predictions
were falsified here yesterday.

So the structural prediction only: **narrowing the matrix operand reduces DSP per
multiply, and there are 22 multipliers across two cores.** How far is a
measurement, and the measurement is cheap -- a `-MapOnly` on `zhao_geom_project`
with the matrix width parameterised, minutes rather than the hours a fit costs.

## 4. The move that is actually available

Add a `MATW` parameter to `zhao_project_core` **defaulting to 32, so nothing
ships differently**, and take a MapOnly at 20. That:

* changes no shipped behaviour and no contract until someone decides to;
* prices the lever with a measurement instead of an inference;
* keeps the owner's control, per CLAUDE.md -- the width becomes a named constant
  rather than a number buried in a function signature;
* is reversible in one edit if the number disappoints.

**And the decision it feeds is genuinely the owner's:** does the view-projection
matrix's entry range get *declared* bounded -- which is what would let the
narrower hardware be correct rather than merely lucky -- in exchange for DSPs on
a budget that is 42 over with 42 blocks still unfitted?

`zhao_project_core.sv` is outside the running island fit's closure, so this is
free to build now. It cannot be *measured* until the fit finishes; one Quartus
process at a time.
