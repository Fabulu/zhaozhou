# Q002 answer — geom2-vdecode-deadlock

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: a9c161150ed64aba8f05d1067ad56fd0
- when: 2026-09-19T20:53:56  seconds: 666  finish: stop  status: ok
- usage: {"completion_tokens": 41775, "completion_tokens_details": {"reasoning_tokens": 39407}, "prompt_tokens": 24054, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 65829}
- inputs: [{"input": "C:\\Users\\Fabs\\AppData\\Local\\Temp\\claude\\C--programmieren-zencrifice\\6dc16d10-374e-4090-84db-3119abad56bf\\scratchpad\\q002-group_seq-at-3dae8f10.sv:1-554", "sha256": "8c6ab35003973085", "chars": 30171}, {"input": "git show 3dae8f10 -- fpga/rtl/geometry/zhao_geom_group_seq.sv", "sha256": "0f5c53bd33f9a8ad", "chars": 12517}, {"input": "git show 3dae8f10 -- fpga/rtl/geometry/zhao_geom_replay.sv", "sha256": "5318c7f25bf24da1", "chars": 6434}, {"input": "git show 3dae8f10 -- tests/geometry/geom_group_seq_directed.cpp", "sha256": "429c8c4bc343afe8", "chars": 8021}, {"input": "git show 3dae8f10 -- tests/mutants/zhao_geom_group_seq_mutant.sv", "sha256": "da94decb861d5ca1", "chars": 15441}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | zhao_geom_group_seq.sv:366,371,450-451 | A hole outside StAlloc/StFill, or more holes than `j_count`, is not charged and can leave the job hanging; correctness relies on upstream one-meshlet timing. | `hole_in_job_c` is only true in StAlloc/StFill; otherwise orphan; `fill_end_c` requires exact equality with `j_count`. |
| 2 | P2 | tests/mutants/zhao_geom_group_seq_mutant.sv:545 | The claimed StSealWait mutation is not visible in the provided diff; only a comment says the drain condition is gone. | The mutant diff hunk `@@ -445,20 +527,21` shows the comment but no StSealWait code body. |
| 3 | P3 | zhao_geom_group_seq.sv:500-501,367 | `vertices_sent_o` counts fan-out accepts, not unique vertices; dual-view is 2x `vi_q`. | It increments on every `a_take_c`, while `vi_q` increments only on `last_slot_c`. |
| 4 | P3 | zhao_geom_group_seq.sv:386-389 | A poisoned handle still reports the original job count, not the sent/landed count; safe use depends on `grp_poison_o`. | `grp_count_o = j_count`; `grp_poison_o = (holes_q != '0)`. |

## ANSWER

### 1. Refusal sequences that still hang or early-seal

Within the stated upstream contract, none of the listed legal sequences hangs or seals while a vertex is still in flight.

- The fill ends when `vi_next_c + holes_next_c == j_count` at zhao_geom_group_seq.sv:371, and that check is evaluated every clock in StFill, not only on a vertex accept, at zhao_geom_group_seq.sv:509-512.
- A hole is charged while `st == StAlloc` or `st == StFill` at zhao_geom_group_seq.sv:366, so first, middle, last, all, and consecutive holes are accounted for.
- The seal waits for landings equal to the vertices actually sent, `vi_q`, per active slot, not `j_count`, at zhao_geom_group_seq.sv:322-326 and zhao_geom_group_seq.sv:515-521.
- A landing is registered unconditionally when `fill_landed_i` is high at zhao_geom_group_seq.sv:432-433. If a landing occurs in the same cycle that `fill_end_c` becomes true, it is updated at the clock edge and then seen by `drain_done_c` in the following StSealWait cycle. It therefore does not cause an early seal.
- If a hole occurs on the same cycle as the last accepted vertex, both `vi_next_c` and `holes_next_c` increment in the same `fill_end_c` evaluation at zhao_geom_group_seq.sv:366-371, so the batch ends with the correct sent/hole count.

The important qualification is Finding 1: if a hole arrives while no job is held, or if upstream sends more holes than `j_count`, the code can orphan the hole or never satisfy the equality in `fill_end_c`. The material says the dispatcher makes orphan holes unreachable at zhao_geom_group_seq.sv:104-107, but the block itself does not enforce that ordering.

### 2. What the sealed group carries, and replay behaviour

For a refused vertex, the sealed group carries:

- `grp_count_o = j_count`, the original job count, at zhao_geom_group_seq.sv:386.
- `grp_poison_o = (holes_q != '0)`, at zhao_geom_group_seq.sv:389.

It does not carry a per-vertex hole marker, and it does not carry the number of vertices actually sent/landed. The running vertex index `vi_q` only counts emitted vertices at zhao_geom_group_seq.sv:356,367-368, so later vertices are shifted into lower indices after a hole. That is why the batch is poisoned rather than partially usable.

The shown replay does not turn that into a stale triangle. It sets `pois_q` from `grp_poison_i` in S_HAND at zhao_geom_replay.sv:580, and in S_TRI a poisoned meshlet is counted and does not go to `S_LOOK`, i.e. it is dropped without a lookup, at zhao_geom_replay.sv:607-612. Therefore the shown replay path cannot read an unwritten/stale arena slot to produce a triangle from a poisoned meshlet.

The residual risk is that the handle itself still reports the original `j_count`. Any consumer that ignored `grp_poison_o` and used `grp_count_o` to bound reads could read slots that were never written. The shown replay does not do that.

### 3. Independence of the new counter / fault flag

There are two different things here, and the answer depends on which one is meant.

- The R31 refusal counter that matters is `holes_o`. It is incremented from `hole_i` when the hole is charged to the held job at zhao_geom_group_seq.sv:447-449. It is not driven by landings, so it does move when the refusal occurs. `holes_o` would fire on the real cause of the original deadlock: a VDECODE refusal.
- `groups_poisoned_o` is not independent of completion. It increments only when a poisoned handle is actually accepted at zhao_geom_group_seq.sv:537-538. In the pre-fix hang, no handle arrives, so `groups_poisoned_o` would not fire.
- `seal_early_o` is not a new R31 counter. It is a sticky fault flag set in StSeal when `!drain_done_c` at zhao_geom_group_seq.sv:524. It is not independently clocked by an external observation; it reuses the same internal drain condition that gates StSealWait->StSeal at zhao_geom_group_seq.sv:515-521. In the fixed design it is structurally unreachable. It would fire in the described mutant where the drain wait is removed, because StSeal would be entered while `landed_q != vi_q`. It would not fire on the original R31 refusal deadlock, because that fault is a hang, not an early seal.

### 4. Directed test asserts correct behaviour

Yes. CASE 7 asserts the correct fixed behaviour, not that the bug occurs.

- It asserts that a batch with one refused record still produces a handle: `hs.size() == 1` at tests/geometry/geom_group_seq_directed.cpp:504-508.
- It asserts that the handle is poisoned: `hs[0].poison == 1` at tests/geometry/geom_group_seq_directed.cpp:510-511.
- It asserts the refused record is excluded from landings: four landings for a count-5 batch at tests/geometry/geom_group_seq_directed.cpp:515.
- It asserts the arena is sealed and counted, and that the seal did not happen while short: tests/geometry/geom_group_seq_directed.cpp:516-519.
- The all-holes, orphan, and clean-after-poisoned subcases assert the handle completes, poison is per job, and no early-seal flag is set.

The test does not assert a bogus `grp_count_o` for the poisoned group; it does not need to, because the design’s correctness mechanism is `grp_poison_o`, not reducing `grp_count_o`.

### 5. Mutant’s substantive change

The provided material does not show the actual StSealWait code in the mutant. It shows only the comment claiming the mutation at tests/mutants/zhao_geom_group_seq_mutant.sv:545. So the exact code change is not shown.

Based on the comment, the mutation is that StSealWait no longer waits for the landing-based drain condition and therefore seals on the accept side while landings are still in flight. If that is accurate, then because the mutant also contains the R31 hole/fill-end logic, it would not reintroduce the original refusal hang. It would instead reintroduce the early-seal fault and make `seal_early_o` fire. It would only reintroduce a hang if it also reverted the R31 fill-end logic, and that is not shown.

## CONTINUATION
- Done: Post-fix fill ends on `vi_next_c + holes_next_c == j_count` (zhao_geom_group_seq.sv:366-371,512), and seal waits for `landed_q == vi_q` per active slot (322-326,515-521). For legal first/last/all/consecutive holes and same-cycle land+hole, no hang/early seal. Handle carries original count (386) and poison (389); shown replay sets `pois_q` (zhao_geom_replay.sv:580) and drops poisoned triangles without lookup (607-612), so it does not produce stale triangles from the shown path. `holes_o` fires from `hole_i` (group_seq:447-449); `seal_early_o` is a sticky StSeal flag (524), not an R31 cause counter, and would not fire on a pre-fix refusal hang.
- Open: Mutant StSealWait body not shown; only comment at mutant:545 claims drain removed, so the mutation and its effect are unverified. Hole protocol assumes no hole in StIdle for a future job and no more holes than `j_count`; otherwise orphan/hang (group_seq:366,371,450-451). `seal_while_short` helper implementation not shown.
- Next chunk: Get full `zhao_geom_group_seq_mutant.sv`, the full directed-test `Env::seal_while_short`, and any mutant driver. Verify StSealWait actually removes `drain_done_c`, run CASE 7 plus `seal_early_o==0` against the mutant, and confirm the driver polarity. Inputs: full mutant file, test Env code, mutant driver.

## Coordinator verdict

**partial** — spot 4: F1 CONFIRMED as a conditional hazard (group_seq:366 charges a hole only in StAlloc/StFill, and the header's one-meshlet-in-flight assumption is stated at :104-107, not enforced here; forwarded to geom2 to prove or enforce). F2 is an INPUT limitation, not a defect: the mutant is the pre-existing seal-early mutant (drain_done_c -> 1'b1), unchanged in substance. F3/F4 are accurate but LOW, and F4 is by design (poison travels with the count). Its continuation correctly noted seal_early_o would not see a refusal hang. No false claims found.
