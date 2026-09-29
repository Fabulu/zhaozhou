# CURRENT STATUS POINTER -- READ THIS BEFORE ANY FIGURE BELOW

**Two conclusions in this file are WITHDRAWN. They are kept for the record and
must not be reused:**

* the **4-7% remaining storage saving** -- withdrawn. It rested on a
  hierarchy-sensitive census and on `bits / 4`, which is capacity arithmetic and
  not an integrated area measurement. **No percentage replaces it.**
* the **51 MHz contractual floor** -- withdrawn. It multiplied a cycle CEILING
  by the frame rate, which inverts the inequality: a ceiling plus a deadline
  gives a relationship, not a minimum clock. The contract's own stated design
  point is **100 MHz**; its "~80 MHz lowest credible" belongs to that cost
  model and is **not** an architecture-independent bound for a redesigned
  engine.

Current position: `design/v2/proposals/R5-agent-candidate.md` and
`design/v2/evidence/uop_store_probe.md`.

---

# V2 evidence base — measured, reproducible, and separated from assumption

Every number the review and R1 rely on is listed here with the command that
regenerates it. A table nobody can regenerate is a number somebody once pasted.

**Baseline refreshed 2026-09-28.** Live head `814687ae955a3ce4690010ce750f2b7b5e597ec2`
on `claude/ceiling-architecture-20260912` — **identical to the head R0 checked**,
so there is no drift between R0's snapshot and this review. Working tree carried
two EOL-only modifications (`spec/form/field-host-image.md`,
`reference/include/zfield/generated/zfield_host_image.hpp`) with zero content
lines changed; they are a schema-generator artefact and affect nothing here.

---

## Source of truth

| | |
|---|---|
| synthesis report | `reports/synthesis/blockpaths/zhao_console_core@current-20260928.map.rpt` (26 MB, gitignored) |
| Analysis & Synthesis | Successful — Mon Sep 28 00:14:57 2026 |
| device | `5CSEBA6U23I7` |
| regenerate attribution | `python tools/budget/map_entity_attrib.py <report> --json attrib.json` |
| regenerate this analysis | `python tools/budget/v2_state_lever.py attrib.json` |

**These are Analysis & Synthesis estimates. This design has never placed**, so
there is no ALM figure and no Fmax from it, and neither may be quoted from it.

---

## 1. What V1 is

| | measured | against `5CSEBA6U23I7` |
|---|---:|---:|
| combinational ALUTs | 293,886 | **351%** of ~83,820 |
| dedicated logic registers | 279,210 | **167%** of 167,640 |
| block memory bits | 3,387,975 | 60% of 5,662,720 |
| DSP blocks | 128 | **114%** of 112 |

Separately reported estimated ALMs: **222,666**.

**The registers alone need ≥69,802 ALM (167% of the part) with the combinational
logic at zero. Memory fits at 60%.** Storage held in flip-flops is what overflows
this device; M10K is where the slack is.

## 2. The state-in-flops lever — R0's central bet, measured

`alut_own`/`reg_own` are EXCLUSIVE and sum to the totals, so they partition.
`alut`/`reg` are subtree totals and must never be added across a hierarchy.

| | |
|---|---:|
| nodes parsed | 1,758 |
| nodes whose subtree holds **zero** memory bits | 941 |
| registers held there | **132,083 (47% of all)** |
| ALUTs held there | **171,861 (58% of all)** |

**The one measured conversion on record** (from the per-entity attribution):
5,181 registers + ~2,698 ALMs → 4,880 M10K bits, **at zero added cycles**. That
is **0.521 ALM recovered per register banked**, and ~0.94 memory bits per
register.

Two consequences, both counterintuitive:

* **Memory capacity is not the constraint.** Banking 60% of the flop-held state
  consumes ~74,646 bits — **7 M10K of 553**. A register is one bit; the device
  has 2.27 Mbit spare. The limiter is **port count** (an M10K has two; a flop
  array can be read by everything at once), not capacity.
* **The lever is large but not large enough.** At its absolute bound — banking
  *every* flop-held register, which is impossible — it recovers **~68,800 ALM**,
  leaving ~153,900 against a 41,910 ceiling. **Still 3.7× over.**

## 3. Concentration — the feasibility fact neither R0 nor phase-3 stated

| touch the top … | ALUTs reached | share |
|---|---:|---:|
| 10 nodes | 61,043 | 20.8% |
| 25 nodes | 100,050 | 34.0% |
| 50 nodes | 141,210 | 48.0% |
| 100 nodes | 187,363 | 63.8% |
| 200 nodes | 235,852 | 80.3% |
| 400 nodes | 277,330 | 94.4% |

**There is no surgical rewrite.** Replacing "the five biggest engines" reaches
about a fifth of the design. This argues *for* R0's systemic approach — a uniform
rule applied to hundreds of nodes is tractable where hundreds of bespoke
optimizations are not — and it means a V2 programme must be costed as a
**400-node migration**, not a handful of engine rewrites.

## 4. What the mass actually is

Classified by whether a node's subtree carries a DSP or any memory bit:

| class | nodes | own ALUTs | share | own registers |
|---|---:|---:|---:|---:|
| carries DSP (arithmetic) | 66 | 73,598 | 25.0% | 75,384 |
| carries memory, no DSP | 781 | 74,912 | 25.5% | 95,261 |
| **neither — glue, control, mux, queues, soft arithmetic** | **911** | **145,376** | **49.5%** | **108,565** |

**Half the machine carries neither a DSP nor a single memory bit.** That is
precisely the "hundreds of independently buffered feature blocks" R0 proposes to
replace with scheduled engines — so R0's thesis is better founded than R0 argues.
Caveat: "no DSP" is not "no arithmetic" — `zhao_field_v3_mulbank` is 3,328 ALUTs
with **8 registers and no memory**, i.e. soft multipliers in logic.

The zero-memory class (§2) and the glue class overlap heavily; **they must not be
added.** The union is bounded by the 171,861 ALUTs of §2.

## 5. Largest single concentrations, by OWN attribution

| node | own ALUTs | own registers | subtree mem bits |
|---|---:|---:|---:|
| `zhao_field_v3_exec:u_exec` | 9,100 | **24,795** | 25,344 |
| `zhao_field_host_v2:u_field_host` | 8,345 | 5,115 | 85,282 |
| `zhao_vertex_arena:u_arena` | 7,553 | 4,491 | 187,308 |
| `zhao_terrain_devstore:u_terrain_devstore` | 7,070 | 4,418 | **0** |
| `zhao_project_core:u_core` | 6,714 | 6,561 | 3,532 |
| `zhao_shell_top_v2:u_shell` | 6,219 | 5,065 | 407,750 |
| `zhao_cmd_exec:u_cmd_exec` | 5,440 | **12,840** | 18,472 |

`zhao_shell_top_v2`'s widely-quoted 59,723 ALUTs is a **subtree** total — it is a
container of ~18 organs. Its own contribution is 6,219.

## 6. Timing — the weakest part of the whole V2 case

| | |
|---|---|
| block fits with a measured Fmax | 109 (74 with `status: ok`) |
| min / median / max | 18.5 / **85.7** / 199.7 MHz |
| ≥ 60 MHz | 58 / 74 (78%) |
| ≥ 100 MHz | **15 / 74 (20%)** |
| ≥ 120 MHz | 3 / 74 (4%) |

These are **leaf blocks fitted in isolation with few pins**. A design's clock is
its **worst** path, not the median, and composition only lowers Fmax.

**The only console-scale placement on record** is
`zhao_console_core@console-core-first-light`: 47,582 ALM, **18.5 MHz**, setup
slack **−44.06 ns**, TNS −39,647 ns. Its caveats are severe and must travel with
it: fitted on the **non-target** sizing device `5CEBA9F31C7`, `ioMode:
virtual-top-ports` with 10,833 virtual pins, and at `sourcesHashed: 106` against
284 today — roughly a quarter of the present design.

It is nonetheless **the only composed timing datum that exists**, and every
frame-budget figure in R0 assumes 60–120 MHz.

## 7. What V1 declares

`design/V1-RELEASE-DEFINITION.md` states guarantees in **gameplay** terms — Duo,
two wizards, persistent destructible island, creatures lit and animated, bounded
lighting/fog/liquids/particles "each to a stated tier". **It contains no numeric
per-frame joint demand vector.** R0 is correct that the legal joint workload is
undefined.

It also declares *"a complete MiSTer-targeted bitstream passing resource and
timing analysis under documented platform assumptions"* — so **whole-console
placement is an existing V1 acceptance criterion**, already answered, and an open
escalation of mine asking whether it still is was over-asking.

## 8. Corrections this evidence forces on our own prior reports

* **`reports/PHASE3-CONCLUSION-20260928-THERE-IS-NO-LARGE-LEVER.md` overreached.**
  Its measurements (case tables, module duplication, parallelism parameters) are
  correct and its ~10% figure stands *for those three classes*. Its closing claim
  that the residual "could only come from function" is **too strong**: it never
  measured the state-in-flops class, which bounds at 58% of ALUTs. The
  distinction is that phase 3 measured **optimizations of V1 in place**, and the
  state lever requires **reimplementation** — which is the V2 programme, not an
  optimization. A correction box has been added to that report.
* The same report's framing remains right in one important respect: nothing
  measured closes a 3.5–6.4× gap on its own.

---

# CORRECTIONS, 2026-09-28, after the R2 review

**Section 2, "the state-in-flops lever", is WITHDRAWN as a budget figure.** The
census is hierarchy-sensitive: it **excludes 147,127 of 279,210 registers** --
more than it counts -- whenever any descendant holds a RAM, including the largest
single holder `zhao_field_v3_exec` (24,795 own registers); and what it counts
includes pipeline and control flops that are not arrays. It is neither an upper
nor a lower bound.

**The measured remaining storage lever is 4-7%, not 31%.** Commit `7d049e9f`
(2026-09-26) recorded that the three conversions that mattered already landed
(`pal_q`, `lodstate st_q`, `forge_assemble pos_q+inv_q` -- ~142,000 bits out of
flip-flops, 2,178-2,458 ALM per M10K) and that the tail runs at ~225 ALM per
M10K. Re-measured at HEAD with `check_ram_inference.py --rank --against`: 291
ranked arrays split **94 not in the composed map / 109 already inferring / 88
live**. The 88 hold 128,466 declared bits, of which the largest (65,536,
`forge_cliff_ram prio_mem_r`) sits in a module with 562 own registers and is not
in flops. Net approximately **63,000 bits, 87 arrays, >=87 M10K blocks,
~15,750 ALM**.

**The binding resource is M10K BLOCKS, not bits.** Quartus packs one array per
block, so 87 small arrays cost 87 blocks against ~238 free. The earlier "7 M10K"
figure was the wrong unit, not a rounding slip.

**Section 4's glue class is not shown to be event-rate.** "No DSP and no RAM"
also catches soft multipliers, dividers and comparators; invocation rates were
never established by those resource counts.

**Section 3's concentration curve stands** -- it is exclusive attribution and
does not depend on the memory classification -- **but it does not bound engine
replacement.** Replacing one coherent engine removes many descendant nodes at
once, so "400 nodes rather than five rewrites" was a false choice.

**Section 7 is corrected.** `design/V1-RELEASE-DEFINITION.md` carries no numeric
envelope, but `design/contracts/FIELD.SEQ.EARTH.md` does: 1,089 lattice vertices
per full patch, 297 four-wide update groups, a 128-association stress frame, and
a frame acceptance ceiling of **<=850,000 Field/Earth-slice clocks**.

**NEW, derived from that contract and stated in neither R0, R1 nor R2:**

    850,000 clocks/frame x 60 frames/s = 51 MHz

**A ratified contract fixes a hard floor of 51 MHz on the Field engine alone**,
at 100% duty, before any other engine and before reserve. The clock is not a free
parameter. The only composed console placement on record is 18.5 MHz.

Raw instrument output: `ram_inference_rank_at_head.txt`.

**ADDED 2026-09-29 -- and this one is a RESULT rather than a withdrawal.** The uop
store's non-inference has a cause and a landed repair: the fetch was nested inside a
gate that was REDUNDANT for that one assignment, and hoisting it makes the 384 x 60
array a Simple Dual Port M10K with no behaviour change. Five mapped reductions, the
before/after numbers, the semantic evidence at the production PLAN=48, the empty
tree-wide sweep for a second instance, and the five things it does NOT mean are all
in [`store_repair.md`](store_repair.md). It is a local V1 repair; it is not a
console saving and no fit has measured it.
