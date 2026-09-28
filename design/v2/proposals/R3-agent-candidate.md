# Zhaozhou V2 — R3 candidate

**Revision:** R3, 28 September 2026. Answers `R2_CANDIDATE.md`; supersedes my
R1 where marked. R0, R1 and R2 are kept intact.
**Baseline:** `0e749b8f` on `design/zhaozhou-v2-rfc` (unchanged during R2's
review, as R2 states). Underlying source head `814687ae`.
**Status:** PROPOSAL. No fit, no schedule, no V2 RTL. **No funded forecast, and
R3 does not produce one.**

---

## 0. What changed, in one paragraph

R1 said the storage lever was worth ~31% and the residual was 108,856 ALM. **Both
numbers are withdrawn.** The storage lever is **4–7%**, because the three
conversions that mattered already landed and what remains is small-array,
block-bound work at ~181–225 ALM per M10K against ~238 free blocks. The residual
was an extrapolation from V1 presented as a V2 requirement, which R2 correctly
refused. What survives from R1 is the concentration curve, the timing worry, and
the demand that claims be earned. **What is new in R3 is a contractual clock
floor of 51 MHz that neither document had, and a much smaller storage lever than
either document assumed.**

## 1. Withdrawn from R1

| R1 claim | status |
|---|---|
| "58% of ALUTs / 47% of registers are the bankable lever" | **withdrawn** — census is hierarchy-sensitive; excludes 147,127 registers, includes pipeline/control flops |
| "storage recovers ~68,800 ALM at its bound (31%)" | **withdrawn** — replaced by 4–7%, measured at HEAD |
| "unfunded residual 108,856 ALM" | **withdrawn as a V2 requirement** — it is a V1 extrapolation, and mixes ALUT savings with ALM deductions without a placed result |
| "half the machine is event-rate coordination" | **withdrawn** — the class also holds soft arithmetic; invocation rates were never measured |
| "400 nodes, not five engine rewrites" | **withdrawn as a dichotomy** — exclusive attribution cannot bound what one engine replacement reaches |
| "no numeric workload envelope exists" | **corrected** — `V1-RELEASE-DEFINITION.md` has none, the **contracts do** |

## 2. Retained from R1

* **The concentration curve** (top 10 = 20.8%, top 50 = 48%, top 400 = 94.4%) —
  exclusive attribution, independent of the memory classification. It bounds
  nothing about engine replacement; it does say the mass is not in a handful of
  fat leaves.
* **Rate-bound vs occupancy-bound** as a budgeting distinction — but see §4: the
  rate side has more flexibility than R1 allowed.
* **Timing is the central risk**, now with a number behind it.
* V1 as oracle at all three faces; `zz2_*` namespace; instruments must be shown
  to fire.

## 3. The storage lever, settled

Commit `7d049e9f` (2026-09-26) recorded it before any of these documents:

| conversion | bits | ~M10K | ALM bought | ALM per M10K |
|---|---:|---:|---:|---:|
| `zhao_geom_drawjob` pal_q (**done**) | 98,304 | 10 | 24,576 | 2,458 |
| `zhao_geom_lodstate` st_q (**done**) | 9,216 | 1 | 2,304 | 2,304 |
| `zhao_forge_assemble` pos_q+inv_q (**done**) | 34,840 | 4 | 8,710 | 2,178 |
| **remaining tail** | ~36,280 | ~40 | ~9,070 | **~225** |

Re-measured at HEAD (`check_ram_inference.py --rank --against`): of 291 ranked
arrays, **94 are not in the composed map, 109 already infer, 88 are live**. The
88 total 128,466 declared bits, but the largest (65,536) sits in a module with
562 own registers and is not in flops. **Net: ~63,000 bits, 87 arrays, ≥87
blocks, ~15,750 ALM — ~181 ALM/M10K.**

**Binding resource: M10K BLOCKS, not bits.** Quartus packs one array per block.
This is why R1's "7 M10K" was not a rounding slip but the wrong unit.

## 4. The rate side is not exhausted — R2 is right and R1 was wrong

R1 argued area-for-time is unavailable on rate-bound paths because R0's geometry
count needs 4–6 lanes. **That reasoning inherits R0's operation count as if it
were a lower bound.** R2's projector identity refutes it:

For lattice coordinates `x = x₀ + i·p`, `z = z₀ + j·p`, a projection row is
`row(i,j) = A·h(i,j) + B·i + C·j + D` — **three products per row become one**,
the affine part stepped exactly with no intermediate rounding before the original
rescale. Verified: the algebra follows from `m₀x + m₁y + m₂z + m₃`, and
**`zhao_project_core` takes arbitrary `vx/vy/vz` and does not exploit it.**

**The principle generalises and R3 adopts it:** *reducing repeated work is not
the same as doing the same work more slowly.* Before any claim that a rate-bound
path is irreducible, the operation count itself must be challenged.

Constraints on this candidate: regular lattice only; accumulator width must reach
i,j = 32 without intermediate rounding (R2's early-rounded failing control is the
right instrument and must be kept); `zhao_project_core` declares **contract
latency fixed 36**, so a variant path is a contract change; the generic path
stays for non-lattice geometry.

## 5. The clock floor — new, and contractual

`design/contracts/FIELD.SEQ.EARTH.md`: **≤850,000 Field/Earth-slice clocks for
the 128-association stress frame.**

    850,000 clocks/frame × 60 frames/s = 51 MHz

**That is a ratified contract fixing a hard floor of 51 MHz on the Field engine
alone, at 100% duty, before any other engine and before reserve.** R0's 60 MHz
leaves ~15% headroom on one engine; the only composed placement on record is
18.5 MHz with −44 ns slack.

**Consequence:** the timing witness is measured against **51 MHz as a contractual
minimum**, not against 60 as a preference. A witness below 51 MHz does not fail a
guess — it fails an existing commitment.

## 6. The experimental programme

R3 **adopts R2's cluster-replacement experiment as the decisive one** and
reorders around it.

| # | experiment | establishes | acceptance |
|---|---|---|---|
| **X1** | **Timing-first execution witness** — accept work → registered context address → RAM response and operand select → registered issue → arithmetic → registered result/status → retirement, with real banks, real arithmetic, real consumers, worst-case backpressure | whether the operand/control organisation can earn its clock | **≥51 MHz** on the real part with real pins. Report the full setup/hold/recovery/removal/pulse-width picture, not an Fmax summary. Failure = revise this implementation, **not** a verdict on V2. |
| **X2** | **Coherent cluster replacement** — one slice (draw/context-binding→geometry, or Field prepared-context) replaced end to end | whether deleting old storage, transport and ownership beats paying for scheduler, banks and adapters | net ALM/M10K/DSP **after** paying for the new machinery; useful throughput held; equivalence against the oracle |
| **X3** | **Envelope extraction and reconciliation** — pull the existing contract commitments (Earth 850k/128-association, geometry rates, texture demand) and state the *joint* question that remains | converts Gate 0 from blank paper to a reconciliation | a proposed joint envelope with only genuine ambiguities flagged for the owner |
| X4 | Internal-vs-boundary path analysis on the 18.5 MHz console | how much of it is virtual-pin artefact | a split of paths by origin, as the texture precedent did (63.63 MHz overall vs 120.37 MHz worst internal) |
| ~~E1~~ | ~~conversion-rate distribution~~ | **dropped** — §3 settles it; the conversions are done |

**X1 and X3 start now. X2 is the one that produces new architectural
information.** R3 provisionally prefers the **Field prepared-context slice** for
X2 over draw/context-binding, because the Field engine carries the largest single
register concentration (`zhao_field_v3_exec`, 24,795 own registers) *and* the
only contractual clock floor — so one experiment tests storage, transport and
timing against a ratified number. R2 invites challenge on that choice; this is
the challenge.

## 7. What makes R3 fail

1. **X1 cannot reach 51 MHz.** Then an existing ratified contract is
   unmeetable and either the contract or the frame rate moves — an owner call.
2. **X2 shows the new machinery costs what the old machinery cost.** The whole
   rearchitecture thesis is then unsupported, and the levers left total under
   10%.
3. **The joint envelope reconciles near the synthetic maxima.** Then full
   capability is not implementable on this part at any architecture.
4. **The projector-style reductions do not generalise.** If the lattice case is
   the only one, the rate side is narrower than §4 hopes.

## 8. Position, stated plainly

**We have neither a demonstrated fit nor a demonstrated impossibility**, and R3
declines to produce another total that looks precise. Three documents have now
each leaned on a lever that measurement shrank: R0's allocation, R1's storage
census, and R2's expectation that storage could yield more. The pattern is
consistent enough to be the main lesson: **on this machine, every estimate made
without an instrument has come in optimistic.**

What is genuinely established: V1 works and is the oracle; the arithmetic and
contracts are real assets; the storage programme is spent; the clock has a
contractual floor of 51 MHz and one measurement of 18.5; and the only move that
produces new information is to build one coherent replacement and measure its
area, ports, throughput and timing **together**.
