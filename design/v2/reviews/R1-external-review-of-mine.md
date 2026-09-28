# Zhaozhou V2 — external review of R1

**Revision:** R2 review, 28 September 2026.  
**Status:** critique and candidate revision, not ratification, implementation authorization, or a fit certificate.  
**Reviewed branch:** `design/zhaozhou-v2-rfc`  
**Reviewed SHA:** `0e749b8fbe090abdf644268555ea5c757869b878`  
**Underlying V1 snapshot:** `814687ae955a3ce4690010ce750f2b7b5e597ec2`.  
**Next review requested:** repository agent challenges these findings, prepares an independent R3, saves it, commits and non-force pushes on the design branch, then returns exact paths and SHA.

## Verdict

R1 made the discussion better. It correctly refuses to infer feasibility from an allocation table, treats timing as a central risk, calls for mixed-workload evidence, preserves the oracle, and recognizes the value of replacing implementation structures rather than deleting capabilities. Those criticisms should change the candidate, not merely be acknowledged.

However, R1's replacement arithmetic is not a derived V2 budget. It promotes a structural heuristic into an object-level classification, a single empirical exchange ratio into an upper bound, and a four-lane example into a claim about every architecture. Those promotions must be removed before their numbers become acceptance gates.

**Agreed:** V1's current complete implementation is much too large; a fit is not established; the architectural reduction is unmeasured.  
**Not established:** 31% is the maximum bankable saving, 49.5% is cheap event-rate control, 400 netlist nodes require 400 separate implementation migrations, or a particular failed prototype ends all full-capability designs.

R2 therefore keeps R0's local-engine direction and R1's emphasis on evidence, but replaces their unsupported area projections with resource ceilings, bottom-up implementation receipts, an explicit unresolved balance, and a timing-first experimental programme. See `R2_CANDIDATE.md`.

## 1. What I accept from the review

1. **The 35k allocation is a target, not a forecast.** R0 did label it provisional, but it made the total too visually prominent relative to the unmeasured engine replacements. Print the reduction ratio beside any future target: 222,666 estimated ALMs to 35,000 requires roughly 84% less by that estimate. Do not relabel a provisional partition as a funded plan.
2. **Timing must shape the architecture.** A frequency sensitivity table does not build a 100 MHz machine. Make memory access, decode, issue, arithmetic and retirement separate physical stages where needed, account for the registers, and measure representative connected paths early.
3. **A joint demand vector is essential.** Existing individual and subsystem guarantees do not automatically specify their whole-console Cartesian product. Missing combinations must become precise questions rather than favorable guesses.
4. **Continuous mapping and checkpointed full fitting are sensible.** Full fits need not follow every source edit. A timing-changing design cannot, however, be declared mature from map-only evidence indefinitely.
5. **Event-rate coordination can be a major consolidation opportunity.** It must be measured as event-rate coordination; resource primitive types alone do not establish that classification.
6. **DSP, logic, RAM and frequency must be optimized together.** A full-width product lane is not synonymous with one DSP block. A memory port saved at the expense of many cycles is not automatically a useful trade.

These are substantive revisions, not a defence of every R0 sentence.

## 2. The 0.521 exchange rate is not an upper bound

R1 extrapolates `2698 / 5181 = 0.52074889` estimated ALM per register removed to 132,083 selected registers. The resulting ~68,782 is an **assumption-conditioned extrapolation**, not an upper bound. There is no theorem or comprehensive measurement that fixes the logic/mux/reset saving per stored bit at that rate.

More importantly, there are additional historical conversion measurements already in the reviewed repository. Commit `86f9a04d3a2ad7b427d419d5707b437a9b878297` records paired map-only results: [S8]

| Historical conversion | Registers removed | Combinational ALUTs removed | Estimated ALMs removed | Estimated ALM / register removed |
|---|---:|---:|---:|---:|
| Forge assemble | 34,840 | 12,845 | 23,391 | ~0.6714 |
| Geometry LOD state | 8,708 | 2,863 | 5,136 | ~0.5898 |

**These savings were implemented before the current baseline. Do not subtract them again.** They provide calibration, and directly disprove using 0.521 as a universal maximum. They do not establish that remaining candidates will achieve comparable ratios. The LOD experiment also explicitly warns that its original timing counter could not resolve the added latency; preserve that qualification.

Storage and control savings overlap. Moving state may eliminate selection networks, duplicate capture registers and enable/reset fanout; consolidating owners may eliminate some of the same logic. The effects cannot be partitioned into an additive 31% storage bucket and a precisely independent ~50% occupancy bucket without mapped attribution.

Similarly, R1 subtracts ~2,000 ALMs for 4,000 ALUTs and ~1,100 ALMs for table logic, marking them measured. Those are neither paired integrated ALM deltas nor safe ALUT-to-ALM conversions. Packing, registers, new arbiters and overlapping transformations matter.

**Required revision:** relabel R1's 150,766 subtotal and 108,856 residual as an illustrative in-place extrapolation, or remove them from the feasibility argument. Keep the much more important statement: no measured replacement design currently closes the gap.

## 3. Zero subtree RAM is a search heuristic, not an inventory of bankable state

The new tool selects `r['mem'] == 0`, where `mem` is a subtree total, then sums exclusive `reg_own`/`alut_own`. That arithmetic avoids a straightforward hierarchy double count, but it does not establish the role of those resources. [S3,S4]

It has false positives for this purpose: pipeline flops, arithmetic state, ready/valid holding registers, counter banks and soft arithmetic can all have zero RAM. Some are candidates; some should stay registers.

It also has false negatives: a module with substantial private flop arrays and any RAM child is excluded. The agent's own table shows `zhao_field_v3_exec` with 24,795 own registers and 25,344 subtree memory bits. The presence of that child memory does not prove all those registers are unavoidable pipeline state. [S2]

The included synthetic counterexample holds exactly 8,192 flop bits and 4,000 ALUTs constant, and merely reparents a 32-bit RAM child. The reported zero-memory register subset changes from 8,192 to zero. This is a reproducible counterexample to the **interpretation**, not an allegation that R1's printed totals were arithmetically fabricated.

**Needed instrument:** a state-object inventory containing width, depth, reset/validity semantics, writers, read addresses per cycle, lifetime, observed/event demand, actual primitive mapping and candidate replacement. Use node rankings to prioritize that investigation. Do not call the ranking itself the investigation.

## 4. No DSP does not mean low-rate control; long-tail nodes do not define the work packages

R1's evidence correctly lists the zero-DSP/zero-memory class as including **soft arithmetic**, then treats its 145,376 ALUTs as independently buffered event-rate control in the architectural conclusion. The evidence does not support that second step. The same document names a 3,328-ALUT multiply bank with eight registers. Its rate is not knowable from its primitive classification. [S1,S2]

Classify work by actual invocation rate, service demand and deadline, with classes such as control, transport, arithmetic, storage, diagnostics and mixed. Let the same logical engine contain multiple classes. An instruction decoder can run every cycle; a descriptor can be visited per triangle; the word "adapter" does not imply thousands rather than millions of events.

The concentration curve sorts **exclusive node attribution**. It says V1 resource usage is distributed across many hierarchy entries. It does not imply that replacing five semantic clusters can reach only five entries: an engine replacement may remove hundreds of descendant entries. Conversely, one engine rewrite can require hundreds of distinct semantic obligations to be covered.

**Required revision:** preserve a many-to-many migration matrix from V1 capabilities/nodes to V2 owners, but do not use netlist hierarchy-entry count as either developer task count or proof that event-rate consolidation dominates.

## 5. Seven M10Ks is not a physical layout

The script's memory estimate is:

`132083 * 0.60 * 4880 / 5181 = 74645.63 bits`.

Even under those assumptions, `ceil(bits / 10240)` is **8**, not 7. The tool rounds the quotient to the nearest integer. Literal relocation of 60% of those register bits is ~79,250 bits and also needs at least eight raw-capacity blocks. All 132,083 bits require at least thirteen raw-capacity blocks.

Those are only capacity lower bounds. The empirical `4880/5181` ratio is not a universal compression law: registers removed from a measured implementation include overhead that is not identical to stored payload bits.

Physical cost depends on width/depth geometry, independent owners, ports, read-during-write semantics, replication, ECC/parity choices if applicable, and buffering. A 32x128 payload contains only 4,096 bits yet an example four-way x32 width-banked implementation uses four RAM blocks to expose 128 bits on a read. [S10]

More than two concurrent read ports does not automatically make a candidate impossible: replication, banking or scheduling may provide them, with an explicit cost. Neither is it free. The existing Field register file is already an example of deliberate RAM-based bandwidth provision.

**Required revision:** replace both "costs 7 M10K" and the blanket ban on register arrays with per-object mapped resource/port budgets. Keep active accumulators, small skid buffers and justified tags in registers. Track MLAB separately because it consumes logic resources.

## 6. Timing: accept the risk, correct what the evidence establishes

I checked the actual retained first-light row. It reports 47,582 placed ALMs, 18.5 MHz, -44.06 ns setup slack, 106 source files, 10,833 virtual pins, and the non-target `5CEBA9F31C7`. It is valid historical evidence for that smaller configuration, not current full-console timing and not a demonstrated lower or upper bound on V2. [S5]

A bad result must not be dismissed as "probably virtual pins." It needs the critical path endpoints, start/end clock, path class, logic vs routing delay, exceptions and provenance. Equally, importing that result as V2's likely clock is unsupported.

The repository already contains an internal-vs-boundary analysis. For example it records `zhao_texture_aux_pipe` at 63.63 MHz reported vs 120.37 MHz based on the worst sampled internal path. Those historical rows are not current capacity certificates either; they demonstrate why the path class matters. [S6]

"Composition only lowers Fmax" is not a general law. Constant propagation, removed I/O paths, restructuring, pipelining and placement can change the critical path in either direction. Nothing guarantees improvement.

The 74-entry statistics need an executable extraction and filtering for selected implementation, source/dependency freshness, device, configuration, actual clock, exceptions, and physical/harness boundary. An unfiltered historical fit collection cannot establish that 80% of the proposed new architecture must be retimed.

Intel/Altera explicitly states that Fmax summary covers particular same-clock paths and is not the complete timing signoff. Check setup, hold, recovery/removal, pulse width, cross-clock paths and unconstrained paths. Lowering clock frequency does not by itself fix hold. [S9]

**R2 change:** implement a timing-first witness pipeline, not merely a minimal shell. Exercise the proposed RAM/decode/operand/compute/retire path and backpressure. Put it in the real platform build, classify every timing failure, and add a real engine to expose load and routing growth. Separate compile/STA from board bandwidth measurements: the former does not require a connected physical board, while validating actual memory service does.

A failed 60 MHz candidate is a redesign signal for that candidate, not proof that no alternative circuit can perform the function. A successful 60 MHz shell likewise does not fund a 100 MHz engine schedule.

## 7. Gate 0 should reconcile existing commitments, not ask for capability from scratch

There is genuinely no established whole-console numeric joint demand certificate in the reviewed material. It does not follow that the workload is completely undefined.

At this very SHA, `design/contracts/FIELD.SEQ.EARTH.md` contains an amended **128-association stress frame**, **1,089 vertices per full patch**, **297 row-packed four-wide update groups per patch**, a **6,000-clock association target**, and an **850,000-clock complete Earth-slice ceiling**. It includes program mix, stalls, cache misses and a later integration obligation with other clients. `reports/Fieldv3.md` explicitly describes eight effects crossing sixteen patches as 128 associations. Current supersession must be checked, but these are not absent requirements. [S7]

R0's deliberately synthetic example used 256 patches times 16 fields times 1,089 points: **4,460,544 evaluations**, thirty-two times `128*1089 = 139,392`. Its illustrative 22 instructions and four lanes are not a proven necessary implementation of all legal programmes.

**A four-lane miss rejects that configuration under those assumptions. It cannot prove impossibility on every architecture.** If a strong impossibility claim is needed, it requires a valid unavoidable-work/bandwidth lower bound and an attainable-hardware upper bound, not a choice of four lanes carried forward from one candidate.

Neither observed p99 nor an observed maximum replaces a contractual guarantee. E3 must validate its counters, preserve time/identity dependencies and construct legal boundary workloads, not merely summarize a pleasant capture.

**Required decision process:** extract the current guaranteed profiles first; identify their authorities and supersessions; list the exact unresolved combinations; propose a coherent joint envelope with consequences. Only those actual product ambiguities go to Fabian. Meanwhile run bounded experiments on indisputable subprofiles. Gate 0 blocks claiming complete feasibility, not all knowledge-producing RTL or tools.

The request already establishes that V1 is the oracle and the goal is a complete working V2 on the intended FPGA. Do not reopen "what is whole-console placement for?" or ask whether shipping V1 at 18.5 MHz is acceptable as a prerequisite to this design exercise.

## 8. Rate-bound does not mean the current amount of work is irreducible

R1 is right to reject blindly serializing busy datapaths. It goes too far in declaring rate-side savings approximately zero.

Exact computation reuse, preparation, narrower proven intermediates, reduced repeated transport and a better algorithm can lower required operations without lowering useful outputs. R0's 98-to-32 divider candidate is one such unvalidated-RTL experiment. Triangle/row preparation reuse is another.

A further candidate is regular-lattice transform factorization. The current projector states a single exact wide sum followed by one rescale, with three required matrix rows and nine coordinate products. On a terrain lattice with `x=x0+i*p`, `z=z0+j*p`:

```
row(i,j) = A*h(i,j) + B*i + C*j + D
A = m_y
B = m_x*p
C = m_z*p
D = m_x*x0 + m_z*z0 + (m_w<<16)
```

Carry B*i+C*j+D exactly, without rounding each step; add the height product, then use the original rescale/saturation and all downstream projection rules. This is not an approximate transform or a claim that V1 lacks all related preparation. Check the existing selected path and prove the accepted coordinate domain first. Arbitrarily warped X/Z or overflow at an earlier specified boundary needs the unchanged fallback. [S11]

The included algebra model checks 836,352 row results with no mismatches; a deliberately early-rounded recurrence fails its negative control. It does **not** execute ZRef or RTL and books no saving. It exists to explain why nine products per terrain vertex is not an architecture-independent lower bound. Under R0's same hypothetical counts, changing that counted term from nine to three products reduces 4,669,056 to 2,996,352, before setup and omitted work. These remain assumptions to replace with actual demand.

## 9. The evidence tool needs repairs before becoming a gate

Two concrete source issues: [S3,S4]

- `v2_state_lever.load_rows()` passes the report's **contents** to `map_entity_attrib.parse()`, which expects a **filename** and opens it. The advertised raw `.map.rpt` path is broken. JSON loading is separate, so this does not invalidate a run made from JSON.
- The anti-vacuity check only rejects a zero ALUT sum. It does not verify exclusive sums equal the root totals, hierarchy uniqueness/completeness, source hash, target or schema. A partial nonempty JSON table can therefore yield precise-looking percentages.

The tool also does not produce the DSP/no-memory class table or timing distribution in the README. Supply those extraction commands and commit their compact input/output evidence, rather than making "every number regenerable" depend on an undocumented local command.

The full map can remain out of Git, but commit compressed exclusive row data, report hash/header, tool revision, and all analysis outputs. A full byte-for-byte map is unnecessary for a reviewer to validate arithmetic and classifications.

Local checks included here reproduce the interface mismatch and the hierarchy counterexample with synthetic inputs. They do not claim the 26 MB map was available or re-run the full census.

## 10. How the next experiments change

Keep the experiments, change the questions and rejection criteria:

- **E0 evidence repair + envelope extraction:** reproducible tables, source authority and classification. Do immediately, without declaring a new product limit.
- **E1 state-layout experiments:** identify real arrays, include high-own-register nodes even if they have RAM children, compare concrete layouts and read schedules. Judge net area, ports, frequency and oracle equivalence. No universal 0.35 ratio or two-port veto.
- **E2 timing-first cluster witness:** real platform constraints plus the intended operand/control path and live traffic. Classify bottlenecks and test a revised implementation before changing the product clock. Separate STA and board throughput.
- **E3 reference demand traces:** actual captures plus legal adversarial profiles, verified counters, structured source hashes and exact time/identity traces. This instrument can fail; injected demand must be detected.
- **E4 semantic cluster replacement:** the missing decisive experiment. Replace one coherent bundle of control, storage, transport and arithmetic. Compare the V1 descendant set against the entire new implementation including scheduler, banks, FIFOs, adapters and diagnostics. Test demand, exact results and placed timing together.

A failed local variant stops or revises that variant. A failed complete capacity budget is reported honestly. Neither failure automatically permits capability cuts or proves every future architecture impossible.

## Scope and source index

This review read R1, its review, decisions, questions, evidence note, analysis script, relevant parser sections, retained timing/source reports and selected contracts/RTL. It did not obtain the uncommitted raw console map, rerun Quartus, execute the complete reference/RTL suite, benchmark ARM, or touch a board. No repository was modified.

All repository paths below are at `0e749b8fbe090abdf644268555ea5c757869b878` unless a specific older commit is named.

- **S1:** `design/v2/reviews/R0-agent-review.md`; `design/v2/proposals/R1-agent-candidate.md`; `design/v2/DECISIONS.md`; `design/v2/OPEN_QUESTIONS.md`.
- **S2:** `design/v2/evidence/README.md`; `reports/synthesis/console_entity_attrib_shipping_20260928.md`.
- **S3:** `tools/budget/v2_state_lever.py`.
- **S4:** `tools/budget/map_entity_attrib.py`, especially `parse(path)` and JSON export.
- **S5:** `reports/synthesis/zhao_block_fit.json`, row `zhao_console_core@console-core-first-light`, source `5e6649dc10cd6c45ddab7f000560a3d39bf21fd2`.
- **S6:** `reports/synthesis/INTERNAL-PATHS.txt`; `reports/synthesis/worst_path_index.json` (selected entries only).
- **S7:** `design/contracts/FIELD.SEQ.EARTH.md`; `reports/Fieldv3.md`; `design/budgets/workloads.yml`; `design/contracts/GEOM.SKIN.md`.
- **S8:** `reports/synthesis/RAM-INFERENCE-LEVER-20260928.md` correction header; commit `86f9a04d3a2ad7b427d419d5707b437a9b878297`.
- **S9:** Altera/Intel `report_clock_fmax_summary` documentation and *Report Fmax Summary* guidance. See `evidence/SOURCES.md` for URLs.
- **S10:** Altera Cyclone V *Embedded Memory Capacity* and *Embedded Memory Configurations*. Mode-specific port details must be checked for actual inferred primitives; the overview table alone is not an SDP/TDP implementation proof.
- **S11:** `fpga/rtl/common/zhao_project_core.sv`, exact row-sum/projection law and implementation commentary.
- **S12:** R0 as preserved in `design/v2/proposals/R0-external-candidate.md` and the provided R0 package; this is the review target, not an authority overriding repository behavior.
