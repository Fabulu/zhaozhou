# C21 MEASURED — **310 ALM, 0 DSP** — and my estimate was 45% too high

**2026-09-04, after the fit.** C21 landed and passes:

    zhao_texture_mosaic   ok   1560s
    alms 310   registers 200   dspBlocks 0   ramBlocks 0

| | before | after | gate |
|---|---|---|---|
| DSP | **4** | **0** | 0 |
| ALM | 197 | **310** | <= 500 |
| registers | — | 200 | <= 350 |
| M10K | 0 | 0 | 0 |
| status | `failed:structure` | **`ok`** | |

**The block that was failing its fit now passes it**, and the two 32x32 wrapping
multiplies by the ratified hash primes are gone.

## THE ESTIMATE BELOW WAS WRONG, AND WRONG IN A WAY THAT MATTERED

It predicted **450-520 ALM** against a 500 gate, called C21 *"on its own
limit"*, and on that basis recommended a **two-cycle shared adder tree** to
halve the cost — a real increase in complexity, a pipeline stage, and a new
`II = 1` argument to make.

The measurement is **310**. The naive combinational CSD was never in danger.

**Why the estimate was high:** it costed 17 independent 32-bit adders at 16-20
ALM each. Quartus does not build them independently — it shares sub-expressions
between the two trees (both shift the same operands), packs carry chains, and
prunes bits that cannot affect the 32-bit result. **A per-adder unit cost is the
wrong model for a constant-multiplier tree.**

## The lesson, which is this repository's own

*"Measurement can remove a BIAS; it cannot choose a VALUE."* The estimate was
useful for deciding the rewrite was **plausible**; it was not evidence about
what to build, and it nearly bought an extra pipeline stage nobody needed.

**An estimate wrong in the safe direction is still wrong** — it just fails by
adding complexity instead of by falling over.

## One number NOT claimed

`fmaxMhz: 79.22`. That is **not** a verdict on the block: QUARTUS_GOTCHAS §12
records that a leaf fit in a mostly-empty device measures ROUTING rather than
the design, and 180 of this fit's pins are virtual. The declared rules for this
target carry no fmax floor, which is why the row is `ok`. The number that will
matter is the composed one.

---

<details>
<summary>The pre-measurement costing, kept because the reasoning is the lesson</summary>

# C21 (the MOSAIC CSD rewrite) costs ~17 adders and lands close to its own gate

**2026-09-04.** `zhao_texture_mosaic` is the one island block whose fit came back
`failed:structure`, and it failed **exactly as its rule says it should**:

    RULE  zhao_texture_mosaic: DSP 4 > allowed 0
    zhao_texture_mosaic   failed:structure   1926.7s   ALM 197

`design/fit_targets.yml` already carries the reason — *"this rule FAILS on
purpose until C21 lands — an inferred multiplier is on the brief's REWRITE
list."* This file costs that rewrite **before** anyone starts it.

## What infers the DSPs

    zhao_texture_mosaic.sv:241   hx_c = $unsigned(m_u) * MOSAIC_CX;   // 73856093
    zhao_texture_mosaic.sv:242   hy_c = $unsigned(m_v) * MOSAIC_CY;   // 19349663

`m_u` and `m_v` are `logic signed [31:0]`, so these are genuine **32x32 wrapping
multiplies** — two DSP pairs, four in total. The wrap is the law (F2), not an
overflow.

## The CSD cost, computed rather than guessed

Constant multiplication costs one shifted add/subtract per **non-zero
canonical-signed-digit**, so the price is fixed by the constants:

| constant | value | binary weight | CSD/NAF weight | adds |
|---|---|---|---|---|
| `MOSAIC_CX` | 73,856,093 | 15 of 27 bits | **11** | 10 |
| `MOSAIC_CY` | 19,349,663 | 12 of 25 bits | **8** | 7 |
| | | | **total** | **17** |

**Seventeen 32-bit add/subtract.** A 32-bit adder on Cyclone V is roughly
16-20 ALM, so **272-340 ALM of adder tree**.

## And that is the problem

    current      197 ALM with 4 DSPs
    + tree       272-340 ALM
    - the logic the multipliers currently need (small)
    ------------------------------------------------
    estimate     ~450-520 ALM     against the brief's gate of <= 500

**C21 lands on its own limit.** It may pass; it may miss by a little. That is a
prediction and this repository does not trust predictions — but it is precise
enough to say the rewrite is **not comfortably inside the gate**, which is worth
knowing before it is written rather than after.

## THE CONSTANTS CANNOT BE CHOSEN FOR CHEAPNESS

The obvious saving — pick hash constants with lower CSD weight — **is not
available.** 73,856,093 and 19,349,663 are the ratified spatial-hash primes, and
the hash decides which mosaic cell a texel lands in. Different constants are a
different picture. That is a ruling, not a tuning knob.

## The options that ARE available, none of them free

1. **Share one adder tree across two cycles.** The two products are independent
   and the block's gate demands `II = 1`, which a two-stage pipeline still
   meets — one tree, twice the latency, roughly half the ALMs. **This looks like
   the answer** and it is what "pipeline" in the brief's own C21 line implies.
2. **Keep the DSPs and change the gate.** Four DSPs against a 16-DSP budget is
   not obviously wrong; the gate says 0 because the brief wanted them elsewhere.
   An owner call, not an engineering one.
3. **Sub-expression sharing between the two constants.** Both trees shift the
   same `m` operands; common partial products may be reusable. Real but
   unquantified, and quantifying it properly means writing the tree.

## What is NOT claimed

That 450-520 is what it will measure. Adder ALM cost varies with packing, the
fitter shares logic this estimate cannot predict, and the existing 197 includes
control the rewrite does not touch. **The number that matters is the fit**, and
the point of this file is that the fit is worth running against option 1 rather
than option 0.

</details>
