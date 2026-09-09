# FIT GATE 3, both halves, same source: the pair-pipe is −1,092 ALM, −2,396 registers and +14 MHz

2026-09-09. Both leaf fits done, clean, seed 1, same tool, same source.

| | `perspuv_svc@gate3-fresh` | `perspuv_pairpipe@regfit` | delta |
|---|---:|---:|---:|
| ALMs | 1,886 | **794** | **−1,092 (−58%)** |
| registers | 3,216 | **820** | **−2,396 (−74%)** |
| Fmax | 105.19 MHz | **119.25 MHz** | **+14.06** |
| DSP | 6 | 6 | 0 |
| RAM blocks | 1 | 2 | +1 |
| block memory bits | 256 | 1,280 | +1,024 |

Against the island's §21.6 overages — 3,337 ALM and 7,285 registers — that is
**32.7% of the ALM overage and 32.9% of the register overage, simultaneously**,
from a module-name change. My pre-registered bracket was 22–32%; the answer sits
at the top of it.

No other candidate found this session moves both failing area criteria together,
and none improves Fmax while doing it.

## The determinism control passed exactly

The second half was run because `fit_targets.yml` asks for "a FRESH svc row from
the same commit". The source blob turned out to be identical to the standing
fit's, so the run became a reproducibility check instead. It reproduced **every
metric to the digit**:

```
                standing (9c787e4a)   fresh (e5cc1f81)
ALMs                  1,886               1,886      identical
registers             3,216               3,216      identical
RAM blocks                1                   1      identical
DSP                       6                   6      identical
Fmax                 105.19              105.19      identical
```

Different commit, different day, different invocation; same source, same pinned
seed, same tool version. That is worth more than this gate: it retroactively
supports every same-source comparison in the ledger, and it means a future
differing row is a real signal rather than noise.

**It does not make single-seed Fmax claims safe.** Determinism across invocations
at ONE seed says nothing about spread ACROSS seeds, and the only three-seed set
this repository has (`rcp24_svc`: 63.93 / 64.89 / 68.63) spans 4.70 MHz. The
pair-pipe's +14.06 MHz is three times that spread, so the direction is safe; the
figure is not a three-seed result and §8.8's ≥125 MHz criterion remains unsettled.

## Both halves breach their budget, and that was the design

`perspuv_svc` breaches ALM (1,886 > 900) and registers (3,216 > 700). The
pair-pipe breaches registers (820 > 700) and M10K (2 > 1) — and **passes ALM,
which svc fails.** The target file called this in advance:

> both rows are expected to violate, and the comparison is between them, not
> against the budget.

So the gate's verdict on the swap is not "the candidate passes". It is: the
candidate breaches fewer rules, by far less, and clears one its predecessor never
has.

## Neither row was stamped, and that is now fixed

Both are labelled rows, so both were written `status: ok` while breaching — the
key-mismatch defect this session found. `run_block_fit.ps1` now calls
`Resolve-FitRules`, and replaying the fixed gate over the already-written rows,
without re-running anything:

| row | stamped in the ledger | the fixed gate |
|---|---|---|
| `perspuv_pairpipe@regfit` | `ok` | **`failed:structure`** — M10K 2>1, registers 820>700 |
| `perspuv_svc@gate3-fresh` | `ok` | **`failed:structure`** — registers 3,216>700, ALM 1,886>900 |
| `zhao_texture_island_v3_top@g2-prod` | `ok` | **`failed:structure`** — registers 16,285>9,000, ALM 10,837>7,500, DSP 17>14 |

The existing rows keep their `ok` because rewriting history is not this fix's
business; `check_fit_rules.ps1` reports the true verdict for them in its labelled
section, and every future run is stamped correctly.

## What this does and does not license

**Established:** at leaf, at one commit, at one seed, the pair-pipe is
substantially smaller and faster than the block it replaces, at identical DSP, and
is a drop-in by ports and parameters.

**Not established:** the composed result. This is a leaf fit with **300 virtual
pins**; the island instantiates the block among real neighbours, and the island's
own composed figure is what the redline is measured against. The swap recipe and
its falsifiers are in `PAIRPIPE-IS-THE-REGISTER-LEVER-20260909.md` — the manifest
edit is an atomic pair, `zero_products_o` is a new port so `zhao_prod_top.sv` must
be regenerated, and `gate3_paired`'s 392 byte-identical records are the
equivalence claim to check rather than assume.

**Not a closure of either gate.** 33% is not 100%. The register breach is systemic
across 9 of 11 §3.3 components, and ALM and Fmax remain separate failures.

**The remaining step is an ISLAND fit, and it is the owner's.** The question it
answers is exact: does the composed island lose ~1,092 ALM and ~2,396 registers,
and what happens to its 82.05 MHz.
