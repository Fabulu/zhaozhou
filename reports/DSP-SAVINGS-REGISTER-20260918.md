# DSP savings register — and the ALM exchange rate

> "we need to save DSPs too, so make sure to at least record the possible savings"
> "Also the 5 DSPs are likely to save more ALMs elsewhere"
> — Fabian, 2026-09-18

## First, a correction I published twice today

I wrote **"DSPs are not the binding constraint"** into
`OWNER-RULING-M10K-CEILINGS-20260918.md` and into two worker briefs. **That is
wrong**, and the evidence was already on disk:

```
reports/BUDGET_HEATMAP.md:318
  "Top-level total: 185 DSP against a 112-DSP device and a policy ceiling of 85-90."
```

**185 against 112 is not over policy, it is over the DEVICE.** The heatmap's own
caveat applies — 185 sums per-module maps that share nothing and includes blocks
never instantiated together, so it is "the arithmetic that exists in the
repository", not the connected console's number. But it is 65% above the physical
part, and `CEILING-FRONTIER-RECONCILIATION` records 192 against the same 112. No
reading of those puts DSP in slack.

The error is the usual shape: the comfortable claim arrived first (memory is the
weapon, so DSP must be fine) and explained almost all the evidence. It cost the
PART.COLLIDE worker a decision — see the revisit below.

## The exchange rate, which is the owner's point and the useful part

**When DSP demand exceeds DSP supply, a freed DSP has an ALM price**, because the
marginal multiplier that cannot get a DSP block is built in logic instead.

POST.COMPOSITE measured that price on its own arithmetic: nine signed 16x8
multipliers in logic cost **~1,200–1,300 ALM**, so

```
one signed 16x8 multiplier in logic  ~=  135 ALM
```

The condition for the exchange to be real is that a logic-built multiplier
actually exists somewhere to take the freed block. At 185 demand against 112
supply there are on the order of **seventy** multipliers' worth of arithmetic
with no DSP to sit in, so the condition holds with enormous margin. It would stop
holding only once total demand drops below 112 — at which point freeing a DSP
buys nothing and this register should be re-read.

**So, provisionally: 1 DSP freed ~= 135 ALM saved elsewhere.**

This is a RANKING rate, exactly like the ~200 ALM/M10K rule, and it closes
nothing. Plan §14.5's qualification applies word for word: useful only on NET
parent measurements. A freed DSP that nothing claims saves zero ALMs.

## What this does to the POST.COMPOSITE trade — both branches now win

The exact product-vector grading table was priced with a condition:

| if the nine multipliers would have been... | direct | via freed DSP | net |
|---|---|---|---|
| **in logic** | −1,200…−1,300 ALM, +250 table | — | **~−1,000 ALM** |
| **in DSP blocks** | +250 ALM (table) | 5 DSP x ~135 = **−675 ALM** | **~−425 ALM** |

I previously called the second branch *"a bad trade"*. **It is not.** It was only
a bad trade under the false premise that a freed DSP is worthless. Both branches
are wins; the first is twice as good as the second. The first fit still has to
say which one happened, but the decision no longer depends on the answer.

## Candidates to record — nothing here is measured in a parent

| # | candidate | DSP | cost | status |
|---|---|---:|---|---|
| 1 | POST.COMPOSITE exact product-vector grading table | up to **5** | 6 M10K, ~250 ALM | **ADOPTED** `15b3d8c3` |
| 2 | PART.COLLIDE quarter-square ROM multipliers | ~**10** | ~100 M10K | **DECLINED ON A FALSE PREMISE — REVISIT** |
| 3 | One shared projector service instead of two private cores | **33** | see below | specified, not applied |
| 4 | `zhao_terrain_normals` six multipliers → one | ~**5** | already done 2026-08-24 | **stale row asserts 18 DSP; re-measure** |

### 2 is the one that changed

The PART.COLLIDE worker declined quarter-square ROMs with the reasoning *"~100
M10K to remove ~10 DSPs, and DSPs cost ~0 ALMs, so it spends a fifth of the
device's memory on the constraint that isn't binding."* The arithmetic was right;
**the premise I gave it was wrong.**

Re-priced at the exchange rate: 10 DSP x ~135 = **~1,350 ALM** for ~100 M10K
(18% of 553). Against the ~200 ALM/M10K ranking rule that is ~13.5 ALM per M10K
— **well below the bar**, so it still does not rank well as an ALM trade. But it
is now the single largest DSP lever on the list, and with DSP over the physical
device, *fitting at all* may matter more than ranking. **Not re-decided here:
recorded as a live candidate whose decision rested on a false premise.**

### 3 is the largest and it is already specified

`@cheque-price`: one shared projection service measured **6,598 ALM / 33 DSP**
against ~12,400 ALM / 66 DSP for two private cores. That is **33 DSP and ~5,800
ALM in one change** — the largest single item known, and the plan makes it part
of P2 completion rather than a later optimisation (*"both real clients draw
through one projector… with old private cores absent from the elaborated
production root"*).

The standing caution stands: those are leaf numbers from two arrangements with no
common frame, and the roadmap says so plainly — *"Not measured at all: the machine
with the projector actually shared."*

## What is NOT claimed here

* No number in this register is a fitted measurement. Every one is a leaf figure,
  a shape calculation, or an estimate.
* The 135 ALM/DSP rate is derived from **one** block's multiplier width (signed
  16x8). A wider multiplier costs more in logic and a narrower one less, so the
  rate should be re-derived per candidate rather than applied flat.
* Savings may not be added together where they remove the same work, and a
  candidate relying on slack another candidate consumes counts once (plan §14.5).

---

## DEBITS — changes that SPEND DSP, recorded beside the savings

A register that only lists savings is the flattering half of a ledger.

| # | change | DSP | why it is still right |
|---|---|---:|---|
| D1 | Lighting service refactor to II2 (`Zhaozhou_Lighting_Emergency_Rescue_2026-09-18`) | **+~9** | planning estimate, not fitted. Buys 48.1x -> inside the frame envelope. The old zero-DSP arithmetic "was bought with time that this workload does not have"; the alternative — cloning the 147-clock scalar engine to reach the rate — costs far more ALMs AND more DSPs. **SUPERSEDED by D1a: the block is built and its multipliers are now COUNTED rather than estimated.** |
| D1a | `zhao_light_stream` + `zhao_light_div32_ii2` + `zhao_light_isqrt64_ii8` + `zhao_light_skin_adapter`, built and qualified 2026-09-18 | **5 multiplier operators; ~5–7 DSP blocks expected, UNFITTED** | see below |

### D1a — what is COUNTED and what is still a guess

The debit is recorded at the level the source can actually support, which is
**multiplier operators**, not DSP blocks. Grepping the four new files for `*`
outside comments and outside elaboration-time constant expressions gives
**exactly five**, all in `zhao_light_stream.sv`:

| lanes | operator | width | what it serves |
|---|---|---|---|
| 2 | `l0p_q <= l0a_c * l0b_c`, `l1p_q <= l1a_c * l1b_c` | **signed 32 x 32 -> 64** | the three dot products per light term AND the three normal squares per normal, on one eight-clock calendar |
| 3 | `c0p_q/c1p_q/c2p_q <= c*a_c * cur_ndl_q` | **unsigned 20 x 17 -> 37** | RGB gain on one clock, RGB emission on the next |

`zhao_light_div32_ii2.sv` and `zhao_light_isqrt64_ii8.sv` contain **zero**
multipliers — they are restoring recurrences, adders and subtractors only. The
folding (16 divide cells reused twice, 4 root cells reused eight times) is what
keeps them that way.

**The DSP-block number is NOT counted and is not claimed.** Cyclone V offers
27x27 and paired 18x19 modes; a signed 32x32 does not fit either, and a 20x17
exceeds the 19-bit operand of the paired mode. A plausible mapping is 2 blocks
per wide lane and 1 per narrow lane, i.e. **7**; Quartus may partition
differently, and `multstyle` or retiming could move it either way. The
emergency report's planning figure was ~9 (6 wide + 3 narrow). **Both are
arithmetic on a datasheet, not a fit.** The first `quartus_map` of this
subsystem replaces this row, and nothing here should be added into a census
until it does.

Three things this row deliberately does NOT do:

* **It does not net out the old owner.** `zhao_geom_light` is still in the tree
  and still instantiates `zhao_terrain_shade`. Until the scalar datapath is
  removed from the selected machine, the honest position is that the console
  now contains BOTH, and the debit is gross rather than net.
* **It does not claim the three colour multipliers "already exist somewhere".**
  No shared provider schedule has been shown for them.
* **It does not price the ALM side at all.** Muxes, the 16 divide cells, the 4
  root cells, the 16-slot arena, three queues and ~28 counters all consume
  ALMs, and the folded structure limits replicated *arithmetic* without saying
  anything about the rest. At the ~135 ALM/DSP exchange rate above, spending 7
  DSP is worth roughly 945 ALM of logic-built multiplier avoided — which is a
  RANKING statement and closes nothing.

**What it buys, measured, not estimated:** the ruled 120,000-normal /
480,000-term fixture completes in **960,086 clocks at II = 2.0002 clk/term**,
against 80,160,000 clocks for the scalar owner — an **83.5x** reduction, inside
both the 1,333,333-clock reserved envelope and the proposed 1,000,000-clock
lighting gate. Root jobs are **120,000, one per normal**, not 480,000.

**What it does not buy:** the eight-lights-on-every-vertex workload is a
different workload and is **1,920,086 clocks — 1.15x over the raw
1,666,666-clock frame.** That is asserted as an overrun by
`light_stream_directed`, not relabelled.

Net position after D1 and the adopted savings is **not computed here on purpose**:
every figure in this file is a leaf estimate or shape arithmetic, and adding
estimates across modules is the leaf-versus-census error this campaign has made
three times. The first connected synthesis reports the real number.

---

## MEASURED 2026-09-18 20:51 — the real number is 297, not 185

`zhao_prod_top@whole-console-sizing` Analysis & Synthesis completed:
**Total DSP Blocks: 297.**

| source | DSP | how |
|---|---:|---|
| `BUDGET_HEATMAP.md` | 185 | summed per-module maps |
| `CEILING-FRONTIER-RECONCILIATION` | 192 | summed |
| **this synthesis** | **297** | one Quartus run over the whole selection |

**297 against the 112-DSP target is 2.65x over.** The summed estimates were low
by ~60%.

Note the direction, because it has been consistent all day: I wrote "DSPs are not
the binding constraint", struck it when the owner corrected me, recorded 185/112
as the real position — and **the real position is worse than the correction.**
Every successive DSP measurement has exceeded the estimate it replaced.

This re-prices every candidate above. Against a 185-DSP overage the shared
projector's 33 looked substantial; against 297 it is ~11% of the excess, and
PART.COLLIDE's ~10 is ~3.4%. **No single lever on this list is close to
sufficient**, and the ~135 ALM/DSP exchange rate — derived when the shortfall
looked like 73 blocks and now more like 185 — still holds directionally but its
"is there something to claim the freed DSP" condition is now overwhelming rather
than merely satisfied.

Caveat kept in front: this is the RESOURCE CENSUS top, which the completion plan
forbids calling a console. It is the arithmetic that exists in that selection,
measured properly for the first time instead of summed.
