# Owner decision needed: the RCP V3 swap

2026-09-08. One page. Roadmap packet 5 step 1. **No new measurement is needed to
make this decision** — the facts below are already banked.

## What was claimed

`zhao_raster_rcp24_v3` at TOKW=14: **1034 ALM / 3 DSP / 93.67 MHz**, against the
island's `zhao_raster_rcp24_svc` at **1041 / 6 / 68.46**. Smaller, half the DSP,
25 MHz faster. I reported that as a decision awaiting your approval.

## Why that comparison does not hold

**It is mismatched in two parameters and omits a whole resource class.**

| | svc (the island's block) | v3 (the candidate, as fitted) |
|---|---|---|
| NCTX | **8** | **16** |
| TOKW | **8** | **14** |
| ALM | 1041 | 1034 |
| registers | 1101 | **1478** (+34%) |
| DSP | 6 | **3** |
| **M10K** | **0** | **8** |
| Fmax | 68.46 | 93.67 |

V3 was fitted carrying *twice the contexts* and still read 7 ALM smaller — which
is not "V3 is smaller", it is a different machine. Its lower ALM count is partly
state relocated into eight M10Ks that never appeared in the sentence I wrote.

The run that *would* have been like-for-like, `@island-profile` at NCTX=8/TOKW=14,
died in 38 seconds to a tool defect: `-TopParameters` arrived as one quoted
string, so `NCTX` was set to the garbage `8,TOKW=14` and `TOKW` was never set at
all. That was misfiled as possibly the block rejecting NCTX=8. It was the tool,
it is now fixed, and a guard refuses the mistake in milliseconds.

## The fact that actually decides it

**V3's throughput advantage is context-dependent, and the island runs too few
contexts.** Measured today in simulation, same RTL, same stimulus, only NCTX and
TOKW changed:

| NCTX | saturated clk/recip | vs the serial reference (6.96) |
|---|---|---|
| **8 — the island's** | **5.78** | 17% faster |
| 10 | 4.69 | 33% |
| 12 | 4.38 | 37% |
| 16 — the profile that was fitted | 4.07 | 41% |

At the island's NCTX=8 the tile **does not meet its own declared acceptance
criterion** of under 4.6 clocks per reciprocal. The arithmetic is correct at
every profile — all boundary and random multiplier sweeps pass, 49.94%
negative-correction coverage over 250,000 cases, all 24 exponents — this is not
a defect. A context-parallel reciprocal simply cannot fill its pipeline with half
the contexts.

So buying V3's advertised behaviour means buying **NCTX ≥ 12**: more contexts
than svc's 8, plus the M10Ks. The original framing presented that as a 7-ALM
saving.

## The options

**(a) Keep svc. Revisit only if a later fit shows the reciprocal cone gating.**
Costs nothing now. The composed `@pktC` worst path did enter `rcp24_svc`, which
is suggestive — but that receipt was taken from a dirty tree and is unquotable,
so the honest version of that argument waits for the next anchored worst-path
census.

**(b) Qualify properly: two leaf fits at the SAME profile, NCTX=12/TOKW=14.**
Twelve rather than sixteen because twelve is the *measured* point where V3 first
meets its own criterion, so it is the cheapest profile at which the swap is worth
anything; four spare contexts nobody needs is exactly the unpriced area the
original comparison hid. Report ALM, registers, DSP **and M10K** together. Cost:
two leaf fits, roughly 20–40 minutes each, and they can run while island work
continues.

## Recommendation

**(a), for now.** Not because V3 is bad — the DSP 6 → 3 halving is real and is the
one claim that survived both mismatches intact — but because the case for
spending fits on it rests on a throughput advantage the island's own
configuration cannot collect, and nothing yet shows the reciprocal is the
limiter. If the next anchored island census puts the gating path inside
`rcp24_svc`, option (b) becomes worth its two fits immediately.

**What I will not do without you:** swap RCP into the island, or change NCTX on
the island's `rcp24_svc` instantiation. Both are capacity/allocation trades.

## Status of the swap, stated plainly

It is **neither recommended nor refused — it is unmeasured at the profile that
matters.** Presenting it earlier as a decision awaiting your approval overstated
what was known.
