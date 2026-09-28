# Zhaozhou V2 — R1 candidate architecture

**Revision:** R1, 28 September 2026. Supersedes nothing; answers
`design/v2/proposals/R0-external-candidate.md`, which is kept intact.
**Status:** PROPOSAL. Not ratified, not an implementation order, not a fit.
**Baseline:** `814687ae955a3ce4690010ce750f2b7b5e597ec2` on
`claude/ceiling-architecture-20260912` — the same head R0 checked.
**Evidence:** `design/v2/evidence/README.md`. Every number below is marked
**[M]** measured, **[B]** a bound derived from a measurement, or **[A]** assumed.

---

## 0. The one sentence R0 is missing

**V2 asks for a 6.4× reduction in estimated ALMs (222,666 → 35,000) at full
capability, and the largest mechanism anyone has measured bounds at 31% of
that.** [M][B]

Everything else in this document follows from taking that sentence seriously
instead of writing an allocation table that sums to a number someone chose.

---

## 1. What is agreed, and is now measured rather than asserted

R0's diagnosis is right. The evidence base strengthens it:

| finding | value | kind |
|---|---:|---|
| registers in zero-memory nodes | 132,083 (47% of all) | [M] |
| ALUTs in zero-memory nodes | 171,861 (58% of all) | [M] |
| nodes with no DSP **and** no memory | 911 nodes, 145,376 ALUTs (49.5%) | [M] |
| memory cost of banking 60% of that state | ~74,646 bits = **7 M10K of 553** | [B] |
| measured conversion rate (one case, favourable) | **0.521 ALM per register banked** | [M] |
| ALMs recovered at the absolute bound | ~68,800, leaving ~153,900 vs 41,910 | [B] |

**Half the machine carries neither a DSP nor a memory bit.** V1 is large because
it is *many distinct machines with private buffers*, exactly as R0 says — and
because 279,210 registers alone demand 167% of the part before any logic.

## 2. The reduction is funded from three places, and only one is measured

| source | mechanism | share of the 84% that must go | kind |
|---|---|---:|---|
| **Storage** | flop arrays → banked M10K; identities in transit | ≤31% | [B] |
| **Occupancy** | replace private per-feature blocks with scheduled engines | unmeasured, must supply ~50% | **[A]** |
| **Rate** | time-multiplex throughput datapaths | **≈0 — already spent** | [B] |

**The rate column is the finding.** R0's own geometry count (4,669,056 products)
needs 4–6 perfect lanes at 100 MHz before normal/light work, clipping, operand
ports or stalls. At 60 MHz with R0's 20% reserve there are 800,000 usable cycles
and four lanes need 1,167,264 — it fails. **You cannot pay for area with time on
the rate-bound paths, because the time is already committed.**

So the V2 bet is precisely: **the occupancy column is real and worth ~50%.** That
is one assumption carrying the whole architecture, and R1's job is to make it
falsifiable early rather than to hide it in a table.

## 3. The organising distinction R0 lacks

Split every capability into one of two classes and budget them apart:

**RATE-BOUND** — work whose quantity scales with vertices, fragments, lattice
points or field evaluations. Geometry products, projection, skinning, raster
per-pixel, Field lane issue, texture sampling.
*Rules:* sized from the ratified envelope (Gate 0), **not** area-optimised by
serialization, arithmetic shared only where the combined deadline is proven,
DSP-mapped deliberately.

**OCCUPANCY-BOUND** — work whose quantity scales with *events*: command decode,
resource/version management, residency and eviction, page and list management,
descriptor validation, fault accounting, scheduling, adapters, queues.
*Rules:* **no private state arrays**, **no private buffers**, banked storage with
a shared port budget, and a **small number of microcoded sequencers** in place of
bespoke per-feature logic.

The concentration curve [M] says the occupancy class is where the mass is: the
top 50 nodes are only 48% of ALUTs and you need **400 nodes for 94%**. A V2 is a
**400-node migration under a uniform rule**, not five engine rewrites. That is
the honest shape of the programme and it must be costed that way.

## 4. Engine boundaries

Accepting R0's model, with ownership stated:

| engine | owns | class |
|---|---|---|
| `zz2_cmd` | command decode, resource versions, frame transactions, uploads, faults | occupancy |
| `zz2_geom` | pose, skinning, normals, transform, projection, cull/clip, setup | rate |
| `zz2_field` | all ratified profiles and op semantics, prepared values, long ops | rate (issue) + occupancy (programs) |
| `zz2_terrain` | one authoritative composed state; residency, deformation, publication | occupancy (+ rate at lattice) |
| `zz2_raster` | binning, edge rules, six attributes, depth, translucency, resolve | rate |
| `zz2_tex` | formats, palettes, sampling/filtering, recipes, detail, refusal | rate (hit) + occupancy (fill) |
| `zz2_plat` | scanout, audio, input, memory control, host bridge, capture | mixed, always-on |

**State ownership rule:** exactly one engine may write any given semantic state;
readers take versioned snapshots. **No engine holds an array in flip-flops** —
that is the rule the whole area case rests on, and it should be a *checked* rule
(see §8), not an aspiration.

**Transport:** compact work references (identity + version + small immediate),
expanded locally by the consumer. **Caveat carried forward from the review:** this
trades area for memory traffic and latency on a platform whose memory service is
unmeasured. It is contingent on E2.

## 5. Allocation — derived, with the shortfall shown

R0's §8 chose 35,000 and split it. R1 refuses to do that. Instead:

**Start:** 222,666 estimated ALM [M]. **Ceiling:** 41,910 [M]. **Must remove:**
180,756 (81%).

| step | ALM | kind |
|---|---:|---|
| V1 as measured | 222,666 | [M] |
| − storage lever at its bound | −68,800 | [B] |
| − duplication consolidation | −2,000 | [M] |
| − case tables → ROM | −1,100 | [M] |
| **subtotal, every measured lever fully cashed** | **150,766** | [B] |
| **ceiling** | **41,910** | [M] |
| **UNFUNDED RESIDUAL** | **108,856 (3.6× over)** | **[A] must come from occupancy restructuring** |

**This table is the deliverable.** It does not balance, and saying so is the
point. A V2 is viable only if replacing private per-feature blocks with scheduled
engines removes ~109,000 ALM — a 72% cut of what remains after storage. **No
measurement anywhere supports or refutes that number today.** E1 and the Gate-B
skeleton exist to produce it.

**DSP:** V1 is at 128 against 112 [M] — over before V2 starts. Treat DSP and
ALUTs as one jointly-optimised resource: `zhao_field_v3_mulbank` is 3,328 ALUTs
with 8 registers [M], soft products that could absorb spare DSPs; conversely
DSP-heavy blocks can decompose into logic. Do not allocate the two independently.

**M10K:** not a constraint for the storage lever (7 of 553) [B]. It *is* a
constraint for working sets and caches; those are sized at Gate 0, from the
envelope.

## 6. Gate 0 — ratify the workload envelope before anything is allocated

**`design/V1-RELEASE-DEFINITION.md` states guarantees in gameplay terms and
contains no numeric joint demand vector** [M]. Until one exists, every allocation
in R0 and R1 is arbitrary, and R0's own synthetic conjunction (4,460,544
point-field evaluations — 18.4× over at four lanes) shows the envelope decides
feasibility more than the architecture does.

Gate 0 produces, for the *declared* V1 capabilities:

* per-frame demand distributions (p50 / p99 / max) for vertices, skinned
  vertices, triangles, fragments, field evaluations, lattice updates, texture
  accesses and memory bytes;
* the **legal simultaneity contract** — which maxima may co-occur;
* a named worst-case profile that V2 must meet, and named profiles it need not.

**This is a product decision, not an engineering one.** It is the single highest-
leverage input in the programme and the first thing to put to the owner.

## 7. The three discriminating experiments

### E1 — the conversion-rate experiment *(decides the whole budget)*

Convert the six largest zero-memory flop arrays to banked M10K **in isolation**,
map-only. Candidates [M]: `zhao_geom_ladderbank` (5,951 reg), `zhao_terrain_devstore`
(4,418 reg / 7,070 ALUT), `zhao_field_v3_dispatch` (4,108), `zhao_field_loader`
(2,815), `zhao_terrain_fieldlist` (2,800), `zhao_part_terrain_tap` (2,783).

**Acceptance:** a *distribution* of ALM-recovered-per-register-banked, replacing
the single 0.521 point. Record for each: ALMs before/after, M10K consumed,
**concurrent read ports required**, and cycles added.
**Kill criterion:** if median recovery < 0.35 ALM/register, or if ≥3 of 6 need
more than two concurrent read ports, the storage lever is materially smaller
than assumed and the entire allocation must be re-derived before any RTL.
**Cost:** ~20–40 min each, map-only, no board. Cheapest decisive test available.

### E2 — the composed-timing and memory-service experiment *(can void everything)*

Fit the **smallest honest composed skeleton** — `zz2_plat` + one engine + the
real memory controller — on the actual `5CSEBA6U23I7`, **with real pins, not
virtual-top-ports**.

**Acceptance:** gating Fmax with real I/O; measured SDRAM/HPS read and write
service under a synthetic mix of the transport model's identity-fetch traffic;
measured fabric↔HPS bandwidth and latency.
**Kill criterion:** if a minimal skeleton cannot hold **60 MHz** on the target
part, every frame-budget number in R0 and R1 is void and the programme restarts
from a lower clock. The only composed datum today is 18.5 MHz with −44 ns slack
on a non-target device at ¼ scale [M].
**Note:** this also measures whether "carry identities, fetch locally" is
affordable, which R0 assumes.

### E3 — the workload-envelope experiment *(feeds Gate 0)*

Instrument the **reference model**, not the RTL, over the real captures and emit
the per-frame demand vector.

**Acceptance:** p50/p99/max distributions per resource family over the actual
content, plus the adversarial scenarios (`ordinary terrain + army + multiple
materials`, Duo, heavy destruction).
**Kill criterion:** none — this cannot fail, it can only inform. It is listed
third only because it needs no FPGA and can run in parallel with E1.

**Order:** E3 and E1 start immediately and concurrently; E2 needs board access
and should be scheduled the moment a skeleton exists.

## 8. Checked rules, not aspirational ones

Every architectural rule that carries budget must have an instrument, because
this repository's own law is that a rule nobody checks is a rule that decays:

* **"No engine holds an array in flip-flops"** → extend `tools/budget/v2_state_lever.py`
  into a gate over the `zz2_*` manifest: any node with registers above a
  threshold and zero memory bits fails the build.
* **"One writer per semantic state"** → generated from the manifest, checked.
* **"Rate-bound blocks declare a deadline"** → a `zz2_*` block without a declared
  rate contract fails registration.
* Each gate must be **shown to fire** on a planted violation before its silence
  is quoted — the broken-instrument law.

## 9. What makes this candidate fail

Stated plainly, as R0 asks:

1. **E1 returns a low conversion rate.** Storage was the only measured lever;
   if it is worth 15% rather than 31% the residual becomes unreachable and V2 as
   specified is dead at full capability.
2. **E2 cannot hold 60 MHz composed on the real part.** Then the frame budget
   shrinks, the rate-bound datapaths need *more* parallelism, and area and time
   move in opposite directions — the one combination with no escape.
3. **Gate 0 ratifies an envelope near the synthetic maxima.** R0's own
   conjunction is 18.4× over at four lanes; an honest envelope that keeps every
   maximum simultaneously is not implementable on this part at any architecture.
4. **The occupancy bet is wrong.** If scheduled engines replace 911 glue nodes at
   only 2× density rather than the ~4× needed, the design lands near 130,000 ALM
   — a better machine that still does not fit.
5. **Memory traffic replaces logic as the ceiling.** "Carry identities" moves
   cost into a memory service nobody has measured on this board.

Any one of 1, 2 or 3 ends the full-capability V2 on this part. **None of them is
an argument for starting the RTL before they are answered.**

## 10. What V1 is for, and why it is not discarded

V1 stays the oracle at all three faces R0 names, and the reason is concrete:
it is the only artefact that knows what the machine must *do*. Its measured
deficiencies — 351% of ALUTs, 18.5 MHz composed, 114% of DSP — are statements
about its *implementation*, not its semantics. The integration defects found in
it this week (the Field index-68→4 truncate-then-check, the PARAMWALK flag with
no consumer) are exactly the adversarial cases V2 must inherit as tests rather
than as behaviour.

**Do not destroy the V1 checkout, its branch history or its uncommitted work.**
Pin it by commit; tag only through the normal workflow.
