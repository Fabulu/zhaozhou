# G1 closeout — what the composed measurement decides, and what it hands over

**Date** 2026-09-05
**Basis** `reports/G1D-COMPOSED-ISLAND-20260905.md` — the first composed
measurement of the texture island that has ever existed.

This is a decision brief, not a proposal. It separates what the number
**settles**, what it **reopens**, and what it **does not touch** — because the
roadmap's G1-D is "compose and measure", and a measurement that does not change
what happens next was not worth two and a half hours of fitter time.

---

## 1. The number

```
alms 7720 of 41910   registers 11790   ramBlocks 18 of 553
dspBlocks 17 of 112  virtualPins 889   fmax 69.05 MHz
```

| | ALMs | |
|---|---|---|
| architecture nominal (§3.3) | 6,600 | +1,120 (+17.0%) |
| **redline** | **7,500** | **+220 (+2.9%)** |
| sum of standalone rows | 7,913 | −193 (−2.4%) |
| **composed** | **7,720** | |

Function is separately proven: 64 fragments in, 64 out, every block's counter
moving, 11/11 checks.

---

## 2. What it SETTLES

### 2.1 The island fits the device comfortably on area

7,720 ALMs is **18.4% of the 41,910-ALM device** and **20.5% of the 37,719-ALM
10% reserve**. DSP is 17 of 112; M10K is 18 of 553. On raw capacity the island
is not the problem, and no survivors decision needs to be driven by fear of
running out of fabric.

### 2.2 "Standalone sums overstate" is not a usable rule

The composed island is **2.4%** under the sum of its standalone rows. That
correction changes no decision the sum would have driven. **For area, the sum
was already good enough**, and the report that argued otherwise says so now.

The rule survives where it is dramatic rather than marginal: the census sums to
**342 DSP against a 112-DSP device**, and the composed island uses 17. Summing
standalone DSP rows is meaningless; summing standalone ALM rows is approximate.

### 2.3 Variant A LOGIC2 works

`zhao_texture_material_combine_v1` synthesises at **2 DSP** against §3.4's
"reject DSP > 2". The refuted II=1 block measured 8. §15.5's claim that two
attributed multipliers in ALM logic is achievable is now measured, not asserted.

---

## 3. What it REOPENS

### 3.1 The survivors decision, with a real number for the first time

`G1-ISLAND-SURVIVORS-20260905.md` asked which components survive against a
7,500-ALM redline using a 7,913 sum it correctly distrusted. The composed
figure is **7,720 — over the redline by 2.9%**.

That is a small enough overrun that it is a CHOICE rather than a forced cut.
Three shapes of answer, and this brief deliberately does not pick between them:

* **Accept and amend the redline.** +2.9% on a block that is 18.4% of the
  device, with the reserve four-fifths untouched.
* **Recover it from the two blocks already over their own §3.3 lines.**
  `zhao_texture_fragrob` is 1,676 against 900 and `zhao_raster_perspuv_svc` is
  2,204 against 900. Those two carry 2,480 ALMs of overrun between them —
  eleven times the island's excess.
* **Cut a component.** Nothing in the composed measurement argues for this and
  it is listed for completeness.

### 3.2 The composed acceptance floor

**69.05 MHz against 105 MHz — 34% short.** This is the finding that decides
whether G1 can close at all, and it is not marginal.

The cause is located and is NOT the obvious one. All twelve worst paths start
at a virtual pin, which looks like a measurement artefact; splitting all 2,000
summarised paths by origin refutes that:

| starting | count | worst slack | implied fmax |
|---|---|---|---|
| virtual pin | 405 | −4.482 ns | 69.05 MHz |
| **inside the design** | 1,595 | **−3.63 ns** | **~73.4 MHz** |

Removing the boundary entirely buys about **4 MHz**. The limit is
`zhao_raster_rcp24_svc`: `u_rcp|c_val[6]~DUPLICATE -> u_rcp|c_m.raddr_a[n]`.

**The structure is a priority scan into a memory address.** `pick_i` is
computed combinationally by a round-robin scan over all `NCTX` contexts testing
`c_val[idx] && c_pend[idx]`, and that result then indexes `c_m`, `c_x`, `c_w`
and `c_ph` — so a valid bit walks eight levels of priority logic and arrives at
a memory's read-address port. `~DUPLICATE` shows the fitter already replicated
the register trying to shorten it.

**This is what a composed fit is for.** RCP24 measures fine as a leaf and
becomes the island's limit only once placed among ten neighbours competing for
the same fabric. No per-block row contains this path.

---

## 4. What this brief does NOT do, and why

**It does not restructure RCP24.** The obvious fix is to register `pick_i` so
the scan and the memory read are in different cycles. That changes arbitration
timing in a block whose throughput is CONTRACTUALLY CONSTRAINED — ruling R7
requires ≥ 1.64 products/clock and `raster_perspuv_svc_directed` currently
measures 1.99. A latency change there could spend the whole margin, and the
right shape is §15.5's own: build both variants behind one interface and let
the fitter choose, not edit the block and hope.

**It does not compare 69.05 with the shell's 99.34 MHz.** Different design,
different constraint set. Putting them side by side invites the mismatched-pose
comparison `CLAUDE.md` records — the one where a ratio measured across two
different poses moved a value confidently in the wrong direction.

**It does not claim 105 MHz is unreachable.** One path is named. Closing it has
not been attempted, and one named path is a starting point, not a verdict.

---

## 5. The three questions this hands to the owner

1. **The redline.** 7,720 against 7,500 — amend, recover from fragrob/perspuv,
   or cut? The measurement supports any of them and prefers none.
2. **The floor.** Is 105 MHz the requirement for the ISLAND, or for the
   composed console? The island is measured alone, with 889 virtual pins that
   the machine will not have.
3. **RCP24.** Is a registered-pick variant worth building and fitting, given it
   spends throughput margin ruling R7 constrains?

Nothing here is blocked on those answers. The next actions are already running:
COMBINE.V1's fit for its ALM and fmax, and perspuv's fit to confirm or refute
the per-axis split.
