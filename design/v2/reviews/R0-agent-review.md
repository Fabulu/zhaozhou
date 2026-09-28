# Review of `ZHAOZHOU_V2_CANDIDATE_RFC_R0.md`

**Reviewer:** repository agent (Claude Opus 5), 2026-09-28.
**Baseline:** live head `814687ae955a3ce4690010ce750f2b7b5e597ec2`,
`claude/ceiling-architecture-20260912` — **the same head R0 checked**, so there
is no drift between R0's snapshot and this review.
**Evidence:** `design/v2/evidence/README.md`, all regenerable.

---

## Summary verdict

**R0's central architectural bet is correct, and better founded than R0 itself
argues. R0's resource allocation is not reachable by the mechanisms R0 names,
and R0 never checks whether it is.**

Those two sentences are the whole review. The thesis — bulk state into banked
memory, a small number of locally scheduled engines replacing hundreds of
independently buffered blocks, identities rather than wide payloads in transit —
is the right diagnosis of what makes V1 large. I can now put numbers on it that
R0 did not have:

* **47% of all registers and 58% of all ALUTs sit in nodes holding zero memory
  bits.** State in flip-flops with a selection network around it.
* **Half the machine — 911 nodes, 145,376 ALUTs, 49.5% — carries neither a DSP
  nor a single memory bit.** That is R0's "hundreds of independently buffered
  feature blocks", measured.
* Banking that state costs **almost no memory**: 60% of it is ~74,646 bits,
  **7 M10K of 553**. The limiter is port count, not capacity.

And the objection, equally quantitative:

* The measured exchange rate is **0.521 ALM recovered per register banked** (one
  data point, favourable case). Taken to its **absolute bound** — banking every
  flop-held register, which is impossible — that recovers **~68,800 ALM** from
  222,666, leaving **~153,900 against a 41,910 ceiling. Still 3.7× over.**
* R0's 35,000-ALM allocation requires removing **84%** of the estimated ALMs.
  The largest lever R0 names bounds at **31%**. **R0 is therefore relying on
  engine-replacement for the other ~53%, and offers no measurement of it at
  all.**

An allocation table adds up because its author chose the total. R0 says this
itself — *"No broad implementation campaign begins merely because an allocation
table adds up"* — and then presents the table as §8's centrepiece without
deriving a single row.

---

## The disagreements that matter

### D1. The allocation is a target, not a budget, and the gap is never stated

**R0 never writes down the ratio it is asking for.** 222,666 → 35,000 estimated
ALM is **6.4×**; in ALUTs, 293,886 → ~70,000 is **4.2×**. A reader of §8 sees
eight plausible rows summing to a number under the ceiling and infers
tractability. The document would read very differently with "this table requires
a 6.4× reduction at full capability" printed above it.

**Verdict: modify.** R1 replaces the chosen total with a *derived* one: measured
V1 own-attribution, minus measured conversion rates, with the residual shown
explicitly and labelled unfunded.

### D2. The clock is treated as a parameter; it is the weakest link in the case

R0 sweeps 60/80/100/120 MHz and picks 100 MHz for its geometry arithmetic. The
measured record:

* 74 placed leaf blocks: median **85.7 MHz**, but **only 20% reach 100 MHz** and
  **22% are below 60 MHz**. A design's clock is its **worst** path.
* The only console-scale placement ever completed:
  `zhao_console_core@console-core-first-light`, **18.5 MHz, setup slack −44 ns**,
  on the non-target sizing device, virtual-pin I/O, at ~¼ today's size.

**A 100 MHz V2 is a re-timing programme across ~80% of the measured leaf blocks,
not a parameter.** R0 scopes no such programme.

**This compounds with R0's own geometry number.** R0 counts 4,669,056 full-width
products and finds 4 perfect lanes = 1,167,264 cycles. At **60 MHz** with its own
20% reserve there are **800,000** usable cycles/frame — so 4 lanes **fails**, and
6 lanes (778,176) *barely* passes **with all other geometry work, operand ports
and stalls omitted**. At the only measured composed clock the margin is not close.

**Verdict: unresolved, and promoted to a blocking experiment** (E2 below). If a
minimal composed skeleton cannot reach 60 MHz on the real part with real pins,
every frame-budget figure in both R0 and R1 is void.

### D3. Area-for-time is assumed available; the measurement says it is already spent

R0's engine model implicitly pays for area with time — shared lanes, scheduled
issue, local sequencing. That is the right instinct and it is how the residual
53% would have to be funded.

But R0's own geometry lower bound already needs 4–6 lanes at 100 MHz *before*
normal/light work, clipping, operand-port limits and stalls. **There is little
time left to buy area with on the throughput-critical datapaths.**

The resolution is a distinction R0 does not draw: **the long tail is not
throughput-critical.** Control, sequencing, adapters and queues can be
consolidated aggressively because they are not on the per-vertex or per-pixel
rate. The concentration curve says that tail is where the mass is — top 50 nodes
are only 48%, and you need **400 nodes for 94%**.

**Verdict: accept the mechanism, reject the silence.** R1 separates
*rate-bound* datapaths (where area-for-time is unavailable) from *occupancy-bound*
logic (where it is the whole lever), and budgets them differently.

### D4. The legal joint workload is the actual blocking question, and R0 defers it

R0 flags this correctly — *"write that as an unresolved contract question rather
than assuming either every maximum simultaneously or only a friendly game
frame"* — and then allocates anyway, which makes every row arbitrary.

I verified the premise: `design/V1-RELEASE-DEFINITION.md` states guarantees in
**gameplay** terms (Duo, two wizards, persistent destructible island, creatures
lit and animated, bounded lighting/fog/liquids/particles "each to a stated
tier"). **There is no numeric per-frame joint demand vector anywhere.** R0 is
right that it is undefined.

This is not an engineering unknown. **It is a product decision** — what must the
console guarantee simultaneously? — and it changes the answer by more than any
architectural choice in the document. R0's own synthetic conjunction
(4,460,544 point-field evaluations, 18.4× over at four lanes) is the proof: the
envelope, not the architecture, decides feasibility.

**Verdict: promote to Gate 0.** No allocation should be ratified before it. R1
does this.

### D5. DSP is already over budget and is allocated without analysis

V1 measures **128 DSP against a 112 ceiling — 114%, over before V2 starts.** R0
allocates 96 with no derivation, and correctly warns that "a product lane is not
a DSP block" without then applying the warning to its own row.

Meanwhile `zhao_field_v3_mulbank` is **3,328 ALUTs with 8 registers and no
memory** — soft multipliers in logic. The DSP↔ALUT boundary is a live, unexplored
lever in **both** directions: mapping soft products onto spare DSPs buys ALUTs;
decomposing DSP-hungry blocks buys DSPs.

**Verdict: modify.** R1 treats DSP as a jointly-optimised resource with ALUTs,
not an independent row.

### D6. "Grow a physically fitted V2 from the start" collides with a measured cost

The instinct is right and I support it — late whole-machine size discovery is
exactly what happened to V1. But an island fit here costs **1.5–4 hours** and
`CLAUDE.md` makes batching at subsystem boundaries a standing rule.

**Verdict: accept with substitution.** Use `-MapOnly` (20–40 s per block here)
as the continuous signal and reserve full fits for named gates. Map answers area,
RAM inference and DSP decomposition; only timing and routing need a fit.

### D7. Two things R0 gets right that deserve to be defended explicitly

* **"Do not confuse three meanings of V2."** The `zz2_*` namespace and a separate
  manifest are correct and cheap. The existing `_v2`/`_v3` suffixes belong to
  subsystem histories and colliding with them would be a genuine hazard.
* **The oracle-discrepancy ledger, and refusing to copy V1 bugs.** Its four
  seed examples are real and current — I found two of them myself this week:
  the Field index-68→4 aliasing (`omap_src_c[15:6]`, truncate-then-check before
  a declared refusal) and the PARAMWALK illegal-descriptor flag with no
  consumer. Both are written up in `reports/`. R0 quoting them accurately is
  evidence it read the tree rather than the summaries.

---

## Alternatives, as required

| alternative | verdict | why |
|---|---|---|
| **Unified compute engine** (one machine, microcoded) | **reject as the primary model** | R0's own geometry bound needs 4–6 product lanes at 100 MHz before omitted work. Time-sharing those lanes with Field, raster setup and terrain is precisely the conjunction that explodes in R0's synthetic case (18.4× at four lanes). Area-for-time is unavailable on rate-bound paths. |
| **Host-heavy partition** (push work to the HPS A9 pair) | **modify — underexplored, and the part supports it** | `5CSEBA6U23I7` is a Cyclone V **SoC** (the DE10-Nano/MiSTer part) with a dual-core Cortex-A9. R0 confines the HPS to "preparation". The real question is the **fabric↔HPS bandwidth and latency**, which is measurable on the board and is not measured. This is the most promising unexamined direction for the residual 53%. |
| **Incremental V1-cluster refactor** | **reject as a route to fit; accept as the migration mechanism** | `PHASE3-CONCLUSION-20260928` measured in-place optimization at ~10% of the placement gap. But the *state-banking* conversions are individually incremental and independently verifiable — EARTHRAM did one at zero added cycles. Incrementalism cannot reach the target; it is the right way to execute the parts that can. |

---

## Accept / modify / reject on R0's §25 one-page decisions

| R0 decision | verdict | note |
|---|---|---|
| Keep public semantic interfaces and content/capture ecosystem | **accept** | Highest-value preservation in the document. |
| Local engines rather than a universal processor | **accept** | Supported by D3; the unified alternative is refuted by R0's own arithmetic. |
| Bulk state banked, active operands in registers | **accept, and strengthen** | 47% of registers / 58% of ALUTs measured; costs 7 M10K. Strongest item in R0. |
| Carry identities, not full descriptions | **accept with a caveat** | Converts area into **memory traffic and latency** on a platform whose memory service is unmeasured. Needs E2/E3 before it is load-bearing. |
| Keep six pixel attributes; share preparation | **accept** | Capability-preserving, and sharing preparation is occupancy-bound work. |
| Generate Field coordinates, export directly | **unresolved** | Correct in principle; depends on the same unmeasured transport. |
| HPS for preparation, not real-time | **modify** | Too conservative — see the alternatives table. |
| Grow a physically fitted V2 from the start | **modify** | See D6: map-only continuously, fits at gates. |
| **§8 allocation table (35,000 / 96 / 455)** | **reject as stated** | See D1. Not derived, and unreachable by the named mechanisms. |

---

## What R0 does not contain that R1 must

1. The reduction ratio, stated once, in the open.
2. A separation of **rate-bound** from **occupancy-bound** logic, budgeted apart.
3. A timing programme, because 100 MHz is not a parameter here.
4. Gate 0: ratify the joint workload envelope before any allocation.
5. Three discriminating experiments with numeric acceptance criteria.
6. An honest statement of what makes the whole candidate fail.
