# Q006 answer — vattr-fix-recheck

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: f4cf34b596404f7c962423ee2cd0f978
- when: 2026-09-19T23:17:38  seconds: 688  finish: stop  status: ok
- usage: {"completion_tokens": 41324, "completion_tokens_details": {"reasoning_tokens": 37634}, "prompt_tokens": 22930, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 64254}
- inputs: [{"input": "fpga/rtl/geometry/zhao_geom_vattr.sv:1-652", "sha256": "a62b098ee98c6985", "chars": 35646}, {"input": "git show 94add368 -- fpga/rtl/geometry/zhao_geom_vattr.sv", "sha256": "d534f35933acc45f", "chars": 12145}, {"input": "git show 94add368 -- fpga/rtl/geometry/zhao_geom_replay.sv", "sha256": "2b108b3dd90b0a4e", "chars": 9}, {"input": "git show 94add368 -- tests/geometry/geom_vattr_directed.cpp", "sha256": "d486501e32254f87", "chars": 11096}]
- kind: task  continues Q005

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | fpga/rtl/geometry/zhao_geom_vattr.sv:396-397,424,458,606 | F1’s checked hazard is guarded, but a missing or skipped in-store u/v can still stall or reuse stale u/v | `uv_have_c` gates `dq_d_ready`; there is no per-row valid bit, timeout, or vattr poison for a missing u/v |
| 2 | P2 | fpga/rtl/geometry/zhao_geom_vattr.sv:450-463,465-476,641-643 | F2 is not fixed locally: a lookup before `done_o` still reads old memory; the claimed REPLAY done-gate is not shown | read port unchanged; `git show 94add368 -- fpga/rtl/geometry/zhao_geom_replay.sv` reports no diff |
| 3 | P2 | fpga/rtl/geometry/zhao_geom_vattr.sv:483-491,489,494-495 | F3 is partly fixed: `done_o` still asserts with dropped/OOB rows; loss is only visible via `poison_o` | `rows_inc_c` counts `lq_drop_c`; `row_lost_c` drives `poison_o` |
| 4 | P2 | fpga/rtl/geometry/zhao_geom_vattr.sv:434-435,440,620-626 | F6/8 residual: a `lit_valid_i` on the `batch_i` clock is accepted under the previous batch’s lit readiness and colour context | `lit_ready_o` uses `op_n_q`/`views_need_q`; `c_st_q` transition uses `views_need_q`, not a batch-aware value |
| 5 | P3 | fpga/rtl/geometry/zhao_geom_vattr.sv:598-602 | `uv_waits_o` may undercount if more than one depth result can wait concurrently | single `uv_wait_seen_q` flag |
| 6 | P3 | fpga/rtl/geometry/zhao_geom_vattr.sv:537-544,550,566-572,620-639 | `batch_i` does not reset colour/profile state, so boundary landing/colour/profile events rely on the previous batch being fully done | the `batch_i` block resets only views/opens/ordinals/landing/row counters |

## STATUS BY ORIGINAL FINDING

### (1) Landing reads u/v without staged check: FIXED, with residual risk
The reported hazard is fixed for the normal in-order contract. A depth result is not accepted until its in-store index is below `uv_ord_q`:
- `uv_have_c = !dq_in_store_c || (32'(dq_index_c) < 32'(uv_ord_q))`: `zhao_geom_vattr.sv:396`
- `assign dq_d_ready = (w_st_q == W_IDLE) && uv_have_c`: `zhao_geom_vattr.sv:397`
- Row writer takes only on the same handshake: `W_IDLE: if (dq_d_valid && dq_d_ready)`: `zhao_geom_vattr.sv:606`
- u/v stage read address and capture: `zhao_geom_vattr.sv:424,458`
- Waits are counted: `zhao_geom_vattr.sv:598-602`

Residual risk:
- The check is ordinal-count-based, not per-index valid. If a u/v ordinal is skipped but later u/v events increment `uv_ord_q`, an in-store index can appear staged while `uv_mem` still holds stale data: `zhao_geom_vattr.sv:396,554-560`.
- If an in-store landing’s u/v never arrives, the writer waits and `done_o` cannot rise because `dq_idle`/writer-idle/row-count terms are not satisfied: `zhao_geom_vattr.sv:490-491`. There is no vattr timeout or poison for missing u/v. The header says holes poison the batch and are never read, but that mechanism is not shown in this file: `zhao_geom_vattr.sv:28-30`.

### (2) Lookup before `done_o` reads stale rows: NOT FIXED locally / NOT SHOWN system
The vattr read port is still the arena’s one-clock synchronous read with no per-row valid and no `done_o` interlock:
- lookup registered one clock later: `zhao_geom_vattr.sv:641-643`
- read memory every clock: `zhao_geom_vattr.sv:460-462`
- data gated only by `rd_in_q`, not by `done_o`: `zhao_geom_vattr.sv:465-476`

The commit claim that REPLAY cannot look up before the `done_o`-gated handle is not shown. The replay file has no diff in the provided material.

### (3) `done_o` rises with dropped rows: PARTLY FIXED
`done_o` still asserts when rows and landings are equal, including dropped rows:
- dropped landing counted as row: `lq_drop_c`, `rows_inc_c`: `zhao_geom_vattr.sv:483-484`
- `done_o` unchanged in shape: `zhao_geom_vattr.sv:490-491`

But lost rows now poison the batch:
- `row_lost_c = lq_drop_c || ((w_st_q == W_V) && !w_in_store_c)`: `zhao_geom_vattr.sv:489`
- `poison_o` set on `row_lost_c` and cleared at `batch_i`: `zhao_geom_vattr.sv:494-495`
- `poison_o` port: `zhao_geom_vattr.sv:179`

This is safe only if the consumer/REPLAY/composer checks `poison_o` and drops the batch. That consumer behavior is not shown.

### (6) Batch-boundary open/u/v/landing/row counters: FIXED
The fix makes boundary events use the new batch’s zeroed ordinals/counters:
- `op_n_c`, `uv_ord_c`, `lit_ord_c`, `lands_b_c`, `rows_b_c`: `zhao_geom_vattr.sv:262-266`
- u/v staged and counted with `uv_ord_c`: `zhao_geom_vattr.sv:457,554-560`
- open storage/increment with `op_n_c`: `zhao_geom_vattr.sv:547-551`
- landing count with `lands_b_c`: `zhao_geom_vattr.sv:563-565`
- row count with `rows_b_c`, including on `batch_i`: `zhao_geom_vattr.sv:591`

So an open, u/v, landing, or row completion on the `batch_i` clock is counted and addressed as the new batch’s first event in these paths.

### (8) Lit/colour batch-boundary behavior: PARTLY FIXED
The lit ordinal and colour writer’s ordinal now use the new batch value:
- `c_ord_q <= lit_ord_c`
- `lit_ord_q <= lit_ord_c + 1`: `zhao_geom_vattr.sv:624-625`

But the lit acceptance and colour context are not fully batch-aware:
- `lit_ready_o = (c_st_q == C_IDLE) && (op_n_q == views_need_q)`: `zhao_geom_vattr.sv:434`
- `lit_take_c` uses that old-batch readiness on `batch_i`: `zhao_geom_vattr.sv:435`
- colour writer state transition uses `views_need_q` as seen this clock: `zhao_geom_vattr.sv:626`
- colour arena comes from current `sl_arena_q`, which may still be old or be overwritten by a same-cycle open: `zhao_geom_vattr.sv:440`

Therefore a `lit_valid_i` coincident with `batch_i` can be accepted because the previous batch’s opens were complete, and then written into the previous batch’s colour context or a partially overwritten new-batch arena context. The header claims no event coincides in composition, but that is not shown and test J does not cover lit on `batch_i`: `zhao_geom_vattr.sv:254-261`.

## NEW DEFECTS INTRODUCED BY THE FIX

1. **New wait can hang if in-store u/v never arrives**
   - The row writer now blocks a depth result until `uv_ord_q` passes its index: `zhao_geom_vattr.sv:396-397,606`.
   - If that u/v never arrives, `done_o` can remain low because the writer/depth path is not idle and rows are not finished: `zhao_geom_vattr.sv:490-491`.
   - No vattr timeout or poison covers this case. `row_lost_c` covers only drops and OOB rows: `zhao_geom_vattr.sv:489`.

2. **Ordinal-count handshake can false-ready after a skipped u/v**
   - `uv_have_c` only compares the landing index to `uv_ord_q`, not a per-index valid bit: `zhao_geom_vattr.sv:396`.
   - `uv_ord_q` increments on every `uv_valid_i`, even when the specific index was not correctly staged: `zhao_geom_vattr.sv:554-560`.

3. **Lit boundary path is still old-batch-dependent**
   - `lit_ready_o` and colour writer context use old `op_n_q`/`views_need_q`/`sl_arena_q`: `zhao_geom_vattr.sv:434,440,620-626`.
   - This is a boundary hazard not covered by the `_c` fix for lit acceptance.

4. **`uv_waits_o` evidence may undercount**
   - A single `uv_wait_seen_q` bit can represent at most one outstanding wait: `zhao_geom_vattr.sv:398,598-602`.
   - Whether multiple depth results can wait concurrently is not shown; if they can, the counter is misleading.

5. **Colour/profile state not reset at `batch_i`**
   - The batch boundary resets counters but not `c_st_q`, `c_ord_q`, or `prof_seen_q`: `zhao_geom_vattr.sv:537-544`.
   - `prof_seen_q` is cleared only by opens: `zhao_geom_vattr.sv:550`.
   - A boundary landing or colour write can therefore use previous-batch state if the previous batch was not fully done.

No new counter is shown to be unable to fire: `uv_waits_o` is exercised by test I, and `poison_o` is exercised by tests C/G. `poison_o` does clear at the next `batch_i`: `zhao_geom_vattr.sv:594`.

## TEST COVERAGE

- **(1) u/v join:** Case I exercises landings ahead of u/v with `uv_delay=600`, checks `uv_waits_o > 0`, no drops, reference rows, and no poison: `tests/geometry/geom_vattr_directed.cpp:395-461`.  
  Old RTL would fail this behavior: the diff replaces the old unguarded `dq_d_ready`/`W_IDLE` take path with the new `uv_have_c` handshake at `zhao_geom_vattr.sv:397,606`; the old path would have read stale `uv_mem`.

- **(3) dropped/OOB rows poison batch:** Cases A, C, and G check `poison_o == 0` on clean and `poison_o == 1` on drop/OOB: `tests/geometry/geom_vattr_directed.cpp:256-268,284-298,366-378`.  
  Old RTL would fail the new poison checks because `poison_o` did not exist in the old module; the diff adds it at `zhao_geom_vattr.sv:179,494-495`.

- **(6) batch-boundary open/u/v:** Case J puts `batch_i`, the first open, and the first u/v on one clock, then verifies vertex 0’s row: `tests/geometry/geom_vattr_directed.cpp:395-461`.  
  Old RTL would fail this boundary keying: the diff shows the old u/v write used the old `uv_ord_q` where the new code uses `uv_ord_c` at `zhao_geom_vattr.sv:457`.

- **(8) lit boundary:** Not exercised. Test J does not place `lit_valid_i` on the `batch_i` clock. The residual lit boundary path is therefore untested.

- **(2) lookup before `done_o`:** Not exercised as a stale-read case. Test K checks row debt when `done_o` first rises: `tests/geometry/geom_vattr_directed.cpp:176-196`, but it does not issue a pre-`done_o` lookup and assert against the external gating. It also would not by itself prove the old RTL false for dropped/OOB rows, because the old counters already counted those losses for `done_o`.

## CONTINUATION
- Done: F1 fixed for the staged-check hazard: `uv_have_c` gates `dq_d_ready` (`zhao_geom_vattr.sv:396-397`) and `W_IDLE` takes only on the handshake (`zhao_geom_vattr.sv:606`); `uv_waits_o` counts waits (`zhao_geom_vattr.sv:598-602`), exercised by test I (`geom_vattr_directed.cpp:395-461`). F3 partly fixed: `done_o` still asserts with drops/OOB (`zhao_geom_vattr.sv:490`), but `poison_o` marks `row_lost` (`zhao_geom_vattr.sv:489,494-495`), exercised by A/C/G (`geom_vattr_directed.cpp:256-268,284-298,366-378`). F6 fixed for open/u/v/landing/row counters via `_c` (`zhao_geom_vattr.sv:262-266,457,547-591`), exercised by J (`geom_vattr_directed.cpp:395-461`). F8 partly fixed: lit uses `lit_ord_c` (`zhao_geom_vattr.sv:624-625`) but `lit_ready_o`/colour context remain old-batch (`zhao_geom_vattr.sv:434-435,620-626`). F2 not fixed locally: pre-`done_o` lookup still reads old memory (`zhao_geom_vattr.sv:450-463,465-476,641-643`); REPLAY gate not shown.
- Open: Whether REPLAY/composer gates lookups on `done_o` and checks `poison_o`; `u_dq` internals for `dq_idle`/`dq_refused` during u/v wait; whether holes can produce a landing whose u/v never comes; whether lit/landing/profile can legally coincide with `batch_i`; `uv_waits_o` undercount if concurrent waits.
- Next chunk: Add directed cases for lookup-before-`done_o` (expect stale unless gated), missing in-store u/v (expect hang/poison), and `lit_valid_i` on `batch_i`; then inspect GEOM.REPLAY/composer for `done_o`/`poison_o` gating and `u_dq` handshake. Needs replay/composer code, `u_dq`, and the GROUP_SEQ/PROJ_LANE timing contract.

## Coordinator verdict

**partial** — F2/F3 are CLOSED at the composition, outside Qwen's inputs: REPLAY's group handle is admitted only with va_done (zhao_console_core.sv:11745; GROUP_SEQ seal 6588), and va_poison ORs into grp_poison_i (11754). Qwen correctly marked the gate 'not shown' and did not assert it absent. F1 residual (a missing in-store u/v stalls) is by design: a hole never lands, so nothing waits. F4/F6 (lit on the batch_i clock uses old-batch context) are plausible and LOW, queued. F5 (uv_waits undercount) is LOW. No false claims. Lesson: include the composer excerpt when a finding depends on it.
