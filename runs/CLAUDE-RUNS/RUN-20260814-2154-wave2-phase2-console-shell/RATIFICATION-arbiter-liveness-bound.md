# RATIFICATION — VRAM arbiter liveness bound B (D3) is wrong and must be re-derived

*Orchestrator decision, 2026-08-15. Raised by the review-fix agent after making the MEM formal lane actually run for the first time.*

## What was found

The wave-2 plan ratified decision **D3**: every guaranteed client is granted within
`B = G·MAX_BURST + REFRESH_OVERHEAD` = **40 cycles**, frozen as a constant in `fpga/rtl/common/zhao_pkg.sv` and cited by `MEM.VRAM.ARBITER`'s `RTL_VERIFIED` maturity entry.

**The proof had never been elaborated.** When the fix agent made the MEM formal lane real, the arbiter liveness property **fails at B=40** and **passes at 60 / 90 / 120** — true worst case somewhere in **41–60** cycles.

Root cause of the bad derivation: it budgeted **one burst per client turn**, but a 64-byte request is **four** bursts. The formula omitted the bursts-per-request factor.

## Decision

1. **The constant is wrong and changes.** A frozen constant that a proof disproves is not a constant, it is a bug. `B` is re-derived as
   `B = G · bursts_per_request · burst_cycles + REFRESH_OVERHEAD`
   with the real values from `spec/memory_rules.md` (G guaranteed clients, 64-byte request = 4 bursts, the sim-profile burst length, and the refresh steal). **Do not simply set B=60 because 60 passed** — derive it, then confirm the derived value is exactly the tightest passing bound by bisection (find the largest failing B and the smallest passing B; they must be adjacent).
2. **The proof must pass at the derived bound, and fail at bound−1.** Both directions get committed as evidence — a bound that only passes is not proven tight.
3. **`MEM.VRAM.ARBITER` maturity is re-examined.** Its `RTL_VERIFIED` entry cites a liveness property that did not hold. Once the corrected bound is proven (and tight), the entry is re-pinned to the new evidence commit with a note recording the correction. If for any reason the corrected bound cannot be proven, the block **drops to `UNIT_VERIFIED`** until it can — no exceptions.
4. **`spec/memory_rules.md` §D3 is amended** with the corrected formula, the derivation showing the bursts-per-request factor, and the measured tight bound. The old formula is kept as a struck-through note so the error is visible rather than silently rewritten (charter honesty discipline).
5. **Scanout is unaffected in kind but must be re-checked**: it is a strict-priority isochronous client, so its own bound is separate — confirm the corrected derivation does not change the "scanout never starves" argument, and if it does, say so loudly.

## Systemic finding (more important than the number)

This is the **second** block whose maturity rested on a formal proof that had never been elaborated (the first: `MEM.GUARD`, whose ledger entry claimed "Formally proven." while the proof failed and its assertions were vacuous). Two for two, on the same lane.

**Required process fix:** the formal lane must distinguish *"skipped"* from *"never ran"*. A `.sby` task that has never successfully elaborated in CI must be a **hard failure**, not a SKIP, and a block may not cite a formal property as maturity evidence unless the lane recorded a passing run **with cover statements proving the assertions were reachable**. The fix agent already tightened the SKIP condition and added covers for MEM.GUARD; generalise that to every formal task in the repo, and add a check that every `formal:` evidence path in `design/blocks.yml` corresponds to a task with a recorded green run.

## Note on the review's own accuracy

The Fable review diagnosed MEM.GUARD's vacuity as caused by LRM-illegal mixed continuous/procedural driving of `rsp`. The fix agent tested that hypothesis and **disproved it** — the proof passes with the mixed driver restored. The real causes were that the lane had never elaborated, and that `(* anyseq *)` on **locals** does not survive this frontend (it elaborates to constants, so the witness contained no environment signal); the fix was free input **ports**. Recorded because it is a good instance of not taking a reviewer's causal claim on faith, and because the `anyseq`-on-locals trap will bite every future formal harness on this toolchain.

## Also from the same pass (no ratification needed, recorded for the trail)

- **A genuine escape existed in MEM.GUARD**: `blit_span` was unclamped, so `blit_base + blit_span` could wrap 32 bits and the wrapped window admitted writes far outside the map. Now clamped; removing the clamp fails the proof. This is exactly the class of bug the "no writes outside assigned memory" property exists to catch — and it was found the moment the property became real.
- **`mem_hps_bridge_directed` was not a DLL stall** (my earlier diagnosis): it completes every check and prints PASS, then **deadlocks in process teardown**, so CTest saw a timeout with no output. Worked around with a documented `_Exit` after the result is known; the teardown lock is filed at the call site.
- **The refresh property**'s cover passes non-vacuously (~11 min) but its depth-900 BMC does not finish; excluded from the lane with measurements recorded rather than manufacturing a timeout that would look like a pass.
