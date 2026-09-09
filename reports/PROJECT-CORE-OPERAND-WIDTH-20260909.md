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

---

# ADDENDUM, same day: the file already knew, the calibration is MEASURED, and my width was off by one bit in the direction that mattered

Reading further into `zhao_project_core.sv` (lines 160-175) found the question
already answered and better answered:

> "`tools/budget/calibration.json` measures a product at 1 DSP from 8 to 27 bits
> and **3** from 28 to 33. So 11 x 3 = **33 DSPs**, and the map agrees exactly.
> **Width narrowing is where 22 of those 33 are.** ... What that needs is a PROOF
> that 27 bits covers a world coordinate ... belongs to the owner, not to this
> file. `docs/OWNER_DOCKET.md` 2026-08-24 states it as such."

So the census's "~3 DSP per multiply" was not an inference after all -- it is a
**measured** calibration point, and 11 x 3 = 33 agrees with the fit exactly. Good
news for that number, and a reminder that "recorded as an inference deliberately"
is worth re-checking against the tree before repeating it.

## FIRST CORRECTION: 18 bits, not 19

I reported the random test's matrix range as 19 signed bits. It is **exactly 18**:
`int32_t >> 14` spans `[-131072, 131071]`, and signed 18-bit two's complement is
`[-2^17, 2^17-1]` -- the same interval. I took `bit_length()+1` of the magnitude
131072, which is right for a positive bound and wrong at exactly `-2^17`.

One bit, and it is the bit the whole question turns on.

## SECOND CORRECTION: the calibration has ASYMMETRIC points, and they change the plan

`calibration.json` carries a `widthB` field. Twelve points use it, all signed,
one multiplier, `ioreg`:

| a x b | DSP | est. ALM |
|---|---|---|
| 32 x 32 | 3 | 137 |
| 32 x 27 | **3** | 126 |
| 32 x 24 | **3** | 119 |
| **32 x 18** | **2** | 104 |
| 27 x 27 | **1** | 91 |
| 27 x 18 | 1 | 76 |
| 24 x 18 | 1 | 71 |

**Narrowing the matrix operand to 27 or even 24 buys nothing at all.** The cost
only moves at **18**. That is the opposite of what "narrow it somewhat" intuition
suggests, and it is why this needed a table rather than a guess.

So the lever is really two levers of very different difficulty:

| move | DSP across two cores | what it costs |
|---|---|---|
| matrix operand -> **18 bits** | **-18** (54 -> 36) | bounds view-projection coefficients to +-2.0 in Q16.16. **Does not touch the world-size question.** |
| coordinate -> **27 bits** as well | a further **-18** (36 -> 18) | +-1024.0 world units at Q16.16 -- the "hard half" the file names, and it collides head-on with the 8 km terrain goal |

Plus roughly 33 ALM per multiply on the first move, about 594 ALM across eighteen.

**The first move is available without answering the world-size question**, which
is the part the docket has been sitting on since 2026-08-24. That is genuinely
new: the docket entry frames width narrowing as blocked on bounding the playable
world, and the asymmetric points show half the prize is not.

## What it is blocked on instead, and the cheap measurement that would unblock it

+-2.0 is a real constraint on a view-projection matrix, not a formality. A
perspective term is `cot(fov/2)/aspect`, which passes 2.0 at about a 53 degree
vertical field of view and reaches 3.7 at 30 degrees. **An 18-bit matrix operand
caps how narrow the FOV can be**, so the question is no longer "how big is the
world" but "how long is the longest lens", which is a far smaller question.

And the calibration **cannot answer where the real boundary is**, because it jumps
straight from 32x18 to 32x24. Nothing at 19, 20, 21, 22 or 23 has ever been
measured. If 32x22 were still 2 DSP, coefficients could reach +-32.0 and the
constraint would stop mattering entirely.

That is one cheap sweep through `tools/budget/gen_calib.py` -- minutes of
`quartus_map`, not a fit -- and it is the next thing to run when the island fit
releases the toolchain. **Queued rather than started: one Quartus at a time.**

## And the MATW plan survives, narrowed

The parameter is still the right shape -- default 32, nothing ships differently,
the width becomes a named owner-editable constant rather than a number inside a
function signature. What changes is the value worth testing: **18, not 20**, and
whatever the new calibration points say is the highest width that still costs 2.

---

# THE BOUNDARY IS MEASURED, and it is narrower and stranger than hoped

Five points landed 2026-09-09. The question was where between 18 and 24 the DSP
cost steps, because the answer decides whether the matrix-narrowing lever costs
the camera anything.

| a x b | DSP | est. ALM | decomposition |
|---|---|---|---|
| 32 x 18 | **2** | 104 | 2 x `Two Independent 18x18` |
| **32 x 19** | **4** | 117 | **4 x `Two Independent 18x18`** |
| 32 x 20 | 3 | 111 | 2 x independent + 1 x `Sum of two 18x18` |
| 32 x 21 | 3 | 113 | same |
| 32 x 22 | 3 | 115 | same |
| 32 x 23 | 3 | 117 | same |
| 32 x 24 | 3 | 119 | same |
| 32 x 32 | 3 | 137 | same |

## The pre-registered hope is REFUTED

This report filed the question as: *"If 32x22 were still 2 DSP, coefficients
could reach +-32.0 and the constraint would stop mattering entirely."*

**32x22 is 3.** So the constraint does not stop mattering. Only **18 bits** buys
the saving, which is **+-2.0 in Q16.16**, and a perspective coefficient
`cot(fov/2)/aspect` passes 2.0 at roughly a **53 degree vertical field of view**.
The lever costs a real floor on how long the lens can be, and that is now a
measured constraint rather than a suspicion.

## AND 19 BITS COSTS MORE THAN 32

The strangest row in the table, and the most useful one. **32x19 needs 4 DSP** --
two more than 18, and one MORE than the full 32x32.

It is not a mis-measurement; the decomposition says why. At 19 bits Quartus takes
**four** `Two Independent 18x18` blocks and no `Sum of two 18x18`, where every
width from 20 upward takes two independent plus one sum. The 19-bit operand just
clears the 18x18 primitive and lands on a split with no sum form available. The
existing symmetric points corroborate it: 19x19 also shows 4 in its four-operator
configuration.

**The practical consequence is a trap.** Anyone narrowing this operand and adding
"one bit of headroom" over 18 lands on the single worst width in the whole range
-- doubling the DSP cost against 18 and beating even the un-narrowed design. A
monotonic intuition (narrower is never worse) is wrong here, and nothing in the
design would signal it: the fit would simply come back with more DSPs than before
the optimisation.

So the rule for `MATW`, if it is ever built: **18, or do not bother.** 19 is worse
than doing nothing.

## What this does to the lever

Unchanged in size -- **-18 DSP across the two projector cores** -- and now with a
known price: the view-projection matrix coefficients must be bounded at +-2.0,
which bounds the narrowest usable field of view near 53 degrees vertical.

**That is an owner decision and a small, specific one.** Not "how big is the
playable world", which is what the docket has had this blocked on since
2026-08-24, but "is a 53-degree vertical FOV floor acceptable". A camera question,
answerable without a fit.
