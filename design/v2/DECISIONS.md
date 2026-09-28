# V2 decisions ledger

Format per the vacation directive: question; chosen option; reason and
alternatives; constraints/cost; consequences. One row per decision, appended.
**Nothing here is ratified by the owner** — these are the agent's decisions taken
under delegated authority, plus a clearly-marked list of what is NOT the agent's
to decide.

**Baseline for all of them:** `814687ae955a3ce4690010ce750f2b7b5e597ec2`.

---

## D-V2-001 — R0's §8 allocation table is rejected as stated

**Question.** Adopt R0's 35,000 ALM / 96 DSP / 455 M10K allocation as the V2
budget?

**Decision.** No. Replace it with a *derived* budget that starts from measured V1
attribution, subtracts measured lever values, and prints the unfunded residual.

**Reason.** The table is not derived from anything — no row traces to a
measurement — and it requires a **6.4× ALM reduction that R0 never states**. The
largest mechanism R0 names bounds at **31%** of what must go. R0's own text says
"no broad implementation campaign begins merely because an allocation table adds
up"; this applies the rule to R0.

**Alternatives.** (a) Accept and refine the split — rejected, refining an
arbitrary total produces a precise arbitrary total. (b) Refuse to budget until
Gate 0 — rejected, a derived budget with a visible hole is more useful than no
budget.

**Cost/consequence.** R1's table does not balance: **108,856 ALM unfunded**. That
is deliberate and is the programme's central risk statement.

## D-V2-002 — the workload envelope precedes allocation (Gate 0)

**Decision.** No allocation is ratified before a numeric joint workload envelope
exists.

**Reason.** Verified that `design/V1-RELEASE-DEFINITION.md` states guarantees in
gameplay terms with **no numeric per-frame joint demand vector**. R0's own
synthetic conjunction is 18.4× over at four lanes, so the envelope moves
feasibility more than any architectural choice.

**Consequence.** Gate 0 is added ahead of R0's Gate A. **This is flagged as a
product decision for the owner, not an agent decision.**

## D-V2-003 — rate-bound and occupancy-bound logic are budgeted separately

**Decision.** Every capability is classified rate-bound or occupancy-bound;
area-for-time trades are permitted only in the occupancy class.

**Reason.** R0's geometry count needs 4–6 lanes at 100 MHz before omitted work;
at 60 MHz four lanes fail against its own reserve. The time that would pay for
area on rate-bound paths is already committed. Meanwhile the concentration curve
shows the mass is in the long tail (400 nodes for 94%), which is occupancy work.

**Consequence.** The V2 area case rests on the occupancy class, and that is now
stated as one falsifiable assumption instead of being distributed through a table.

## D-V2-004 — `-MapOnly` is the continuous signal; full fits are gated

**Decision.** Accept R0's "grow a physically fitted V2" intent, substitute
map-only as the routine instrument.

**Reason.** Island fits cost 1.5–4 h here and `CLAUDE.md` makes batching at
subsystem boundaries a standing rule; map answers area, RAM inference and DSP
decomposition in 20–40 s. Only timing and routing need a fit.

**Consequence.** E1 is affordable immediately. E2 is explicitly a fit and is
scheduled as a gate.

## D-V2-005 — `zz2_*` namespace and a separate manifest

**Decision.** Accept R0 unchanged.

**Reason.** Existing `_v2`/`_v3` suffixes belong to subsystem histories
(`zhao_field_v3_*`, `zhao_texture_cache_pipe_v2`); colliding with them is a real
hazard, and this tree has already been bitten by composing a superseded version.

## D-V2-006 — architectural rules must ship with instruments

**Decision.** Any rule carrying budget (no flop arrays; one writer per state;
rate-bound blocks declare a deadline) gets a checked gate that is **shown to fire
on a planted violation** before its silence is quoted.

**Reason.** This repository's recurring failure is an instrument that reads zero
because it cannot fire. Four such were found in one pass this month.

## D-V2-007 — the unified-compute alternative is rejected as the primary model

**Decision.** Keep R0's "small number of locally scheduled engines".

**Reason.** A single microcoded engine must time-share the very lanes R0's own
arithmetic shows are already 4–6 deep at 100 MHz; the Field conjunction is 18.4×
over at four lanes. Rate-bound sharing is the one thing the measurements forbid.

**Alternative kept open.** Host-heavy partitioning is *not* rejected — it is the
most promising unexplored direction for the residual, and E2 measures the
fabric↔HPS path that decides it.

---

## D-V2-008 — the storage lever is 4–7%, and D-V2-001's residual is withdrawn

**Question.** After R2 challenged the 0.521 ALM/register figure as not a bound,
is the storage lever larger or smaller than R1 claimed?

**Decision.** **Smaller — materially.** R1's 31% and its 108,856-ALM residual are
withdrawn. The measured remaining lever is **4–7%**.

**Reason.** Commit `7d049e9f` (2026-09-26) already recorded that the flop-array
programme is done: three conversions landed (2,178–2,458 ALM/M10K) and the tail
runs at **~225 ALM/M10K**. Re-measured at HEAD, 291 ranked arrays split 94
absent / 109 already inferring / **88 live**, worth ~15,750 ALM and needing **≥87
M10K blocks** against ~238 free. **R2's two extra data points are rows two and
three of that table — already-spent savings, as R2 itself says.**

**The deeper error was the unit.** Quartus packs one array per M10K, so N small
arrays cost N blocks regardless of total bits. R1's "7 M10K" was not a rounding
slip.

**Consequence.** `tools/budget/v2_state_lever.py` is deprecated as a budget
instrument in its own header; `check_ram_inference.py --rank --against` is the
correct one. **I built a census and read it as a work list four days after
committing a note whose lesson is that its noise is not a work list.**

## D-V2-009 — rate-bound does not mean irreducible (R2 accepted)

**Decision.** Withdraw R1's claim that area-for-time is unavailable on
rate-bound paths, and adopt R2's principle: **reducing repeated work is not the
same as doing the same work more slowly.**

**Reason.** R1 inherited R0's nine-products-per-terrain-vertex count as though it
were a lower bound. R2's identity `row(i,j) = A·h(i,j) + B·i + C·j + D` for
lattice coordinates takes three products per row to one. Verified: the algebra
follows from `m₀x + m₁y + m₂z + m₃`, and **`zhao_project_core` takes arbitrary
`vx/vy/vz` and does not exploit it.**

**Constraints.** Regular lattice only; accumulator width to i,j = 32 without
intermediate rounding (keep R2's early-rounded failing control); `zhao_project_core`
declares contract latency **fixed 36**, so a variant path is a contract change.

## D-V2-010 — the clock floor is contractual, not chosen

**Decision.** The timing witness is measured against **51 MHz as a contractual
minimum**, not 60 MHz as a preference.

**Reason.** `design/contracts/FIELD.SEQ.EARTH.md` sets **≤850,000 Field/Earth-slice
clocks for the 128-association stress frame**. At 60 fps that is 51 MHz for the
Field engine alone at 100% duty, before any other engine and before reserve.
Stated in none of R0, R1 or R2.

**Consequence.** A witness below 51 MHz fails an existing commitment rather than
a guess. The only composed placement on record is 18.5 MHz.

## D-V2-011 — X2 (cluster replacement) replaces E1 as the decisive experiment

**Decision.** Adopt R2's coherent cluster replacement. Drop R1's E1 conversion
sweep. **Provisionally prefer the Field prepared-context slice** over
draw/context-binding.

**Reason.** E1 measured a lever that D-V2-008 shows is spent. The Field slice
carries the largest single register concentration (`zhao_field_v3_exec`, 24,795
own registers) **and** the only contractual clock floor, so one experiment tests
storage, transport and timing against a ratified number. R2 invited a challenge
on its choice; this is it.

---

## Not the agent's to decide — for the owner

| # | question | why it is a product decision |
|---|---|---|
| **P1** | **What must the console guarantee simultaneously?** | Sets Gate 0. Changes feasibility more than any architecture choice. Every allocation is arbitrary without it. |
| **P2** | If the ratified envelope proves unimplementable on `5CSEBA6U23I7` at full capability, **which capability tier moves?** | The directive forbids the agent from shrinking fields, giant support, Gouraud, normal detail, terrain/destruction, Duo or legal capacities. |
| **P3** | Is **18.5 MHz-class composed timing** acceptable for V1 as a shipped artefact, or is V1 an oracle only? | Determines whether V1 needs its own timing programme or is frozen as reference. |
| **P4** | What is whole-console placement **for** — bring-up, demo, or timing evidence? | Carried forward from the retired escalation; still unanswered, still gates nothing. |
