# The lighting arithmetic blocker is retired — verified in the source

The GEOM.LIGHT worker stopped at a stated conflict and refused to edit the shared
core, which was the right call on the evidence it had. **The evidence was stale.**

## What was claimed as the blocker

> "the render core and `skin_world_normal`/`lambert_from_world_normal` are not
> bit-identical laws… Editing the shared core in passing is the move both
> contracts were rewritten to forbid, so it is recorded loudly in the header and
> left for the ruling."

## What the source actually says

`reference/src/zcreature/creature_core.cpp:595`:

```cpp
int32_t lambert_from_world_normal(const int64_t n[3], int64_t mag, int32_t lx,
                                  int32_t ly, int32_t lz) {
  const __int128 dot = ...n[0]*lx + n[1]*ly + n[2]*lz;
  if (dot <= 0) return 0;
  int64_t lam = static_cast<int64_t>((dot + mag / 2) / mag);
  if (lam > 65536) lam = 65536;
  return static_cast<int32_t>(lam);
}
```

**`dot + mag / 2` is round-half-up, and it is literally in the current source.**
The declaration's own doc comment says so too: *"one dot, one round-half-up
divide, clamped at 0x10000."* The rescue package's claim is correct
character-for-character; I checked before acting on it.

The report that described this as a positive-only floor divide is stale, and the
worker inherited it.

## So what actually differs

Not the rounding. **The interface.**

| | renderer | creature |
|---|---|---|
| returns | signed, **unclamped** raw term | clamped to `[0, 65536]` |
| rounding | `floor((dot + floor(mag/2)) / mag)` | `(dot + mag/2) / mag` |
| negative dot | signed result carried forward | early `return 0` |

Same quotient arithmetic underneath, two adapters on top. And the equivalence
closes: for positive dot the same half-divisor is added before division; for
negative dot the signed rounded result is non-positive and the creature clamp
returns zero anyway; saturating the intermediate to signed 32 bits cannot reach
into a final range of `[0, 65536]`.

**No artistic choice is required, and no picture changes.** The terrain
detail-before-clamp rule stays in the terrain adapter, where it already is.

## The bigger correction: magnitude reuse was NOT the fix

The worker's diagnosis — and mine, when I relayed it — was that the 48x gap and
the repeated `isqrt` were the same problem. **They are not.** The shared core's
own schedule:

```
three normal-component squares (byte products)   30 clocks
three normal x light products (byte products)    48 clocks
square root                                      32, OVERLAPPED -- zero extra elapsed
serial division                                  64 clocks
```

**The square root already costs zero additional elapsed clocks.** The cost is 78
byte-product clocks plus a 64-clock serial divide. Removing the redundant
magnitude removes redundant work and leaves the walk and the divider untouched —
*"even an imaginary fourfold improvement to the entire computation would leave a
twelvefold overrun."*

This is the third time today a comfortable diagnosis arrived first and explained
almost all of the evidence. The measurement (II = 167, 48.1x) was right; the
attribution was wrong.

## Verified here, not taken on report

| check | result |
|---|---|
| `SHA256SUMS.json` | **11/11 files verified, 0 mismatched** |
| `python -m unittest discover` | **Ran 9 tests — OK** |
| `verilator --lint-only -Wall zhao_light_div32_ii2.sv` | **0 diagnostics, RC=0** |
| `verilator --lint-only -Wall zhao_light_isqrt64_ii8.sv` | **0 diagnostics, RC=0** |

The lint results are new information: the author states the two RTL candidates
were never HDL-simulated or fitted. They now have one tool's clean opinion, which
is not synthesizability and not a timing result.

## The DSP cost, recorded as a DEBIT

The replacement deliberately spends DSP to buy time: two wide and three narrower
multiplier pipelines, **~9 DSP as a planning estimate, not a fitted count**.

That lands against a device already at **185 DSP demand vs 112 physical**
(`reports/DSP-SAVINGS-REGISTER-20260918.md`), so it is a real debit and is
recorded as one. The justification is explicit and correct: *"The old zero-DSP
arithmetic was bought with time that this workload does not have."* The
alternative — cloning the 147-clock scalar engine to reach the rate — would cost
far more of both.

## What is NOT proven

* **No measured 100 MHz result**, and the package says so itself.
* The II2 / II8 figures are **software cycle-model intervals**, not RTL.
* The ~1,000,000-clock lighting acceptance target is **proposed**, and sits
  inside a 1,333,333-clock envelope — which is not proof the console frame fits.
  Geometry production, memory stalls, point-light providers and attribute
  transport still count.
* **Eight lights on 120,000 vertices = 960,000 terms = 1.92M issue clocks** on
  this variant, i.e. over budget. That must stay an explicit over-budget test and
  must not be hidden behind "supports eight lights".
