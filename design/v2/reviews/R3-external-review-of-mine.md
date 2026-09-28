# Zhaozhou V2 — external review of R3

**Revision:** R4 review, 28 September 2026.  
**Status:** a review and candidate next step, not ratification, feature-cut authority, a fit claim, or a promise of feasibility.  
**Reviewed design branch:** `design/zhaozhou-v2-rfc` at `9ece4a0a978f0b3762545d0162473a4756676d7f`.  
**Underlying RTL snapshot in that branch:** the R3 documents identify `814687ae955a3ce4690010ce750f2b7b5e597ec2`.  
**Companion:** `R4_FIELD_EXPERIMENT_CANDIDATE.md`.

## 1. Verdict and agreement

R3 makes the next step clearer. Accept its preference for the Field prepared-context slice as the first coherent replacement experiment. Accept that the well-known large mechanical RAM conversions are already incorporated in the current baseline. Accept that no revision so far has earned a whole-machine resource or performance forecast. Preserve V1, the reference semantics, content and captures; use them during every replacement step.

The strongest shared conclusion is now operational: build one coherent replacement and measure semantic correctness, actual useful cycle demand, memory ports, fitted clock and integrated area together. Another universal percentage inferred from source shapes will not decide the architecture.

Three new conclusions in R3 need correction before they become gates:

1. `C <= 850,000` is a ceiling on permitted execution cost, not a proof that all implementations require 850,000 cycles. It cannot impose a universal 51 MHz minimum.
2. The claimed 4–7% remaining storage saving is not a measured before/after delta or a complete array-level upper bound. The new classification still uses module-level facts to infer object-level mapping and misses some declarations entirely.
3. “One RTL array always costs one M10K” is false as a universal hardware/tool statement, and in any event is not a constraint on manual V2 memory organization.

Correcting these claims does **not** establish that large savings remain, that V2 will fit, or that timing will be easy. It removes unsupported conclusions in both directions.

## 2. The 51 MHz inference reverses the inequality

The contract says both [S3]:

- Complete 128-association Earth stress frame: at most 850,000 Field/Earth-slice cycles, including the named preparation/publication costs.
- Complete measured slice must finish within 80% of its actually available cycles.

Let `C` be the measured candidate cycle demand under the declared complete workload, `f` its timing-clean operating frequency, and `r = 0.20` the reserve. On the provisional 60 Hz compute model, acceptance includes:

```
C <= 850000
60 * C / f <= 1 - r
```

For an implementation that actually needs 850,000 cycles:

```
f >= 51.00 MHz        without reserve
f >= 63.75 MHz        with 20% reserve
```

Neither is an architecture-independent lower bound. A hypothetical implementation needing 500,000 cycles at 40 MHz consumes 75% of a frame and satisfies both inequalities. This is a mathematical counterexample, **not a claim that such an implementation exists**.

Conversely, a 51 MHz implementation that takes the entire 850,000-cycle allowance consumes 100% of a frame. It would fail the 20% reserve rule. R3's MHz-only acceptance is therefore neither a necessary nor a sufficient test of the required work rate.

Where real frame timing or clock domains differ, use the actual mapped deadlines and inter-domain service times. Do not confuse compute cycles, video-profile ticks and clock frequencies. The first witness must carry an explicit timebase mapping.

A candidate may finish faster than a maximum allowance. Changes to internally fixed pipeline-latency tests are a separate compatibility question: audit which timing is publicly observable, and preserve or explicitly version that behavior. A header saying an old pipeline was 36 cycles does not make its gate count the definition of V2's capability.

**Correction requested:** replace “contractual 51 MHz floor” throughout decisions, open questions, R3 and evidence with “implementation-conditioned frequency requirement derived from measured `C` and the reserve.” Retain the original statements as clearly superseded history, not live conflicting requirements.

## 3. What the storage evidence really establishes

### 3.1 Already-spent savings stay spent

R2 expressly said the additional FLOPARRAY conversions were already incorporated and must not be subtracted again. Their purpose was to refute a universal 0.521 ALM/register coefficient, not to predict a bigger remaining percentage. A coefficient can fail as an upper bound while the available opportunity is nevertheless small. Those are different statements.

R3's historical density table is also not the same measurement as the paired synthesis receipts. For example, `34,840 / 4 = 8,710`; `9,216 / 4 = 2,304`. Those are the values in its density table. The earlier FLOPARRAY commit reports paired changes of 23,391 and 5,136 **synthesis-estimated** ALMs, respectively [S8]. The table based on register capacity is not a substitute for those measured deltas, and neither describes a new saving at current HEAD.

Do not extrapolate either the optimistic empirical ratios or the bits/4 proxy to the whole machine. A register-count floor is not a net area-saving formula: removed mux/control logic, packing with surviving logic, new validity bits and new RAM interface logic all matter.

### 3.2 The replacement scanner still cannot certify individual mappings

I inspected `check_ram_inference.py` at R3 [S4]. Its composed annotation records:

```
module name -> (own register count, subtree memory-bit count)
```

It retains the first row encountered for a module name, rather than preserving all distinct instantiated parameterizations. The rank printer emits `ALREADY INFERRING` when:

```
module_subtree_memory_bits >= this_declared_array_bits
```

That does not establish where *this array* went. A module containing RAM B and flop array A may have enough B memory to falsely certify A. Nor does a lower memory total establish that every declared bit of A became a flop: elaborated parameters, constant/dead elements and packed layouts can differ.

R3's own captured ranking contains examples beyond the 65,536-bit cliff priority store it excludes [S6]:

- `zhao_geom_replay.triq_q`: 12,288 **declared** bits, annotated with only 1,315 own registers and 6,144 memory bits.
- `zhao_part_state.chl_m`: 8,192 **declared** bits, annotated with 682 own registers and 7,360 memory bits.

Those rows cannot be read as 12,288 and 8,192 instantiated flip-flops. This does not identify the exact correct mapping; it identifies another gap between declared shapes and measured objects.

### 3.3 There is an actual declaration class the scan misses

The declaration recognizer starts with `logic|reg|bit`. The Field executor declares [S5]:

```
uop_t store[0:(CTX*PLAN)-1];
```

That declaration does not match the recognizer. At the displayed default parameters (`CTX=8`, `PLAN=32`, `REGS=32`), the packed uop fields describe 15,360 payload bits. **I am not asserting that this store currently uses 15,360 flops.** Its real mapping must be inspected. The point is that the supposed complete storage census does not enumerate this object.

The attached checks exercise that exact regex with the actual declaration and a positive-control `logic` declaration. They also demonstrate the module-total false-certification counterexample. They do not run the whole Quartus mapper or determine the Field store's netlist mapping.

### 3.4 Proper scope for the 4–7% figure

The new scan is useful as a triage aid. It does not justify “the remaining storage lever is settled at 4–7%.” The figure is constructed from a heuristic list and a storage-capacity conversion, not a set of completed replacements with integrated area deltas.

Use instead:

> The known large in-place conversions are already present. The candidate tail appears less attractive. Its remaining net saving is not established by this scan. Broader V2 state-lifetime and memory-organization changes are a separate unmeasured mechanism.

This is not an invitation to reopen all conversions. Inventory the actual storage in the **selected Field slice**, where the experiment already needs this information. Do not turn the whole project into another scanner rewrite.

## 4. Physical memory cost: neither eight blocks nor automatically 87

Bit capacity alone underprices banking, width, replication, ports and fragmentation. R3 is correct to insist on those costs. Its converse statement is too strong.

The Cyclone V handbook explicitly documents M10K packed mode: two eligible independent single-port RAMs can occupy one physical memory block, with Quartus implementing the mode where appropriate. Each logical RAM is limited to half the block, and its shape and mode must be compatible [S9]. This is not proof that the 87 candidates qualify, or that the pinned compiler will pack any particular pair in this project. Verify the actual configuration.

V2 can also explicitly combine tables into one logical address space. For example, sixteen logical `32 x 16` tables contain 8,192 payload bits and can be addressed as one `512 x 16` store using `table_id` and row. This retains capacity, **not sixteen independent simultaneous access ports**. It is useful only where the schedule, locality and update semantics allow shared ports. Arbitration, generation, read/write collisions and the resulting latency are real costs.

Distinguish four cases:

1. An unmodified independent RAM declaration and its current inference result.
2. Device-supported packing of eligible independent memories.
3. A manually coalesced state store with a new, proven access schedule.
4. Reduction of simultaneously live state through lifetime/queue redesign.

R3's simple-array count concerns case 1. The rearchitecture may use cases 2–4, but has not yet measured their net benefit. No new whole-machine savings estimate follows.

## 5. Timing is serious; compare the same work on the same candidate

Keep the historical 18.5 MHz first-light result and its full caveats. It shows a failure of that configuration. It does not characterize an unbuilt, locally pipelined V2. A successful minimal shell would not certify V2 either.

The first Field witness should expose actual address generation, bank reads, selection, arithmetic, dependency bookkeeping, result backpressure and retirement. Do not put the long ready/valid chain back together outside the individually pipelined leaves. Assess setup, hold and other applicable constraints under a real constrained wrapper; do not substitute virtual-port Fmax for internal timing.

A slow or large candidate is valuable negative evidence about **that candidate**. Prefer local, discriminating changes and keep a Pareto table (area, RAM ports, actual cycles, timing-clean clock, total frame time). Avoid broad declarations of impossibility from one threshold miss. Also avoid retaining an expensive candidate simply because it passes a friendly correctness test.

## 6. Field-first is a useful choice, not a concession about feasibility

The prepared-context slice is an excellent first experiment because it contains an actual canonical semantic reference, an existing nontrivial stress profile, wide context/operand transport, long-operation services, and difficult lifetime rules. It may expose whether the architectural bet has value. I accept this choice over starting with draw/context binding.

The reason is **the existing behavioral and workload evidence**, not a universal 51 MHz floor. The 24,795-register executor attribution is a reason to inspect its composition, not a promised saving.

Keep all required profile/op semantics. Do not hardwire the three familiar Earth programs, lose output-presence semantics or use fewer contexts/fields without preserving the declared accepted workload. Program and uniform-value sharing is permitted only for genuinely identical, versioned identities; the worst-case number of distinct plans remains a capacity obligation.

## 7. Scope of owner decisions

Extract the existing guarantees now. A successful Field experiment is not full-console certification; a missing whole-console joint envelope is not a reason to stop that bounded experiment.

The agent should prepare specific proposed combined workloads and their product consequences. Do not ask the owner to derive a microarchitectural traffic vector unaided. Do not silently interpret the lack of a joint vector as either permission to multiply every maximum together or permission to shrink them. Existing capabilities and accepted profiles stay intact during the experiment.

## 8. Evidence that would actually change the outlook

The next important result is a reproducible baseline/candidate pair over the **same complete Field workload**, with:

- semantic outputs, sticky status, presence and required ordering checked against V1/reference;
- current parameters, dependencies, source digests and tool constraints recorded;
- complete useful cycle demand, stalls, bank conflicts, FIFO maxima and memory traffic;
- mapped resource data plus placed timing for the discriminating configurations;
- net resource cost after new scheduler, memory banks, adapters, diagnostics and output ownership;
- a precise list of removed/replaced V1 instances and surviving shared services.

Success would show a real mechanism for V2; it would not multiply into a whole-machine fit estimate. Failure would reject or modify that organization, not erase V1's value.

## 9. Work performed for this review

Read the attached agent transcript and live R3 branch, R3 candidate/review/evidence, Earth contract, actual scanner, raw ranking excerpt and Field executor declarations. Checked the historical `7d049e9f` commit and the earlier paired conversion receipt. Consulted the official Cyclone V packed-memory documentation.

Executed eight small arithmetic/source-excerpt checks in `checks/verify_r3_reasoning.py`. They test the logic of the proposed gates, scanner predicates and address organization. **No new Quartus synthesis, placement, full Field differential or board run was performed.** The uncommitted 26 MB full map was not independently reconstructed. Repository content was not changed.

## Source index

All repository paths below are pinned to `9ece4a0a978f0b3762545d0162473a4756676d7f` unless a historical commit is named. `sources.json` supplies resolvable locations.

- **S1:** `design/v2/proposals/R3-agent-candidate.md`.
- **S2:** `design/v2/reviews/R2-agent-review.md` and `design/v2/evidence/README.md`.
- **S3:** `design/contracts/FIELD.SEQ.EARTH.md`, target throughput and clock-decision sections.
- **S4:** `tools/quartus/check_ram_inference.py`, `DECL`, `composed_index`, and rank annotation.
- **S5:** `fpga/rtl/field/zhao_field_v3_exec.sv`, parameters, `uop_t`, `store`, issue and pipeline state.
- **S6:** `design/v2/evidence/ram_inference_rank_at_head.txt`, leading rows.
- **S7:** historical commit `7d049e9fafb1e18a94e2036e992a22094e2953f1`.
- **S8:** historical paired-map commit `86f9a04d3a2ad7b427d419d5707b437a9b878297`.
- **S9:** Altera, *Cyclone V Device Handbook*, “Memory Blocks Packed Mode Support,” document 683375, section 2.8.
