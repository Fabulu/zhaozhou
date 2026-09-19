# Q007 answer — arbiter-pending-fix

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: b4d67769607e4c53a4d50a318cc2a7f9
- when: 2026-09-19T23:34:38  seconds: 665  finish: stop  status: ok
- usage: {"completion_tokens": 42474, "completion_tokens_details": {"reasoning_tokens": 39185}, "prompt_tokens": 12954, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 55428}
- inputs: [{"input": "fpga/rtl/memory/zhao_hps_arbiter.sv:1-416", "sha256": "1ff93c34671c2c02", "chars": 21760}, {"input": "git show 525a3f6d -- fpga/rtl/memory/zhao_hps_arbiter.sv", "sha256": "f303efaa67c246a9", "chars": 6644}, {"input": "git show 525a3f6d -- fpga/rtl/mem/zhao_mem_upload.sv", "sha256": "753521fbe241b190", "chars": 1258}, {"input": "git show 525a3f6d -- tests/memory/hps_arbiter_n_directed.cpp", "sha256": "f26309b601c3a430", "chars": 5136}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | fpga/rtl/memory/zhao_hps_arbiter.sv:196,296-300 | A pending slot can serve a stale request and suppress a changed live request. | `eff_req` prefers `pend_req` when `pend_v` is set; capture requires `!pend_v`, so a new live request is not stored. |
| 2 | P2 | fpga/rtl/memory/zhao_hps_arbiter.sv:296-300,306-312 | A still-held holder request can be served again after a pending service if the client does not deassert on grant. | After the burst, the same `req_i[i].valid` can be picked again; no per-request “served” flag distinguishes old from new. |
| 3 | P3 | fpga/rtl/memory/zhao_hps_arbiter.sv:380 | Wrapper comment mislabels the starvation counter as rule 5. | It says `rule 5`, while the starvation/waiting rule is rule 7 (lines 99-104,125-134). |
| 4 | P3 | tests/memory/hps_arbiter_n_directed.cpp:569, fpga/rtl/memory/zhao_hps_arbiter.sv:238 | Directed case 9 is not a failing case for the old pending bug; it pins existing A_ACTIVE write-beat behavior. | The commit changed only the comment above the unchanged `b_wr_valid_o = wr_valid_i[owner]`. |
| 5 | P3 | fpga/rtl/mem/zhao_mem_upload.sv:527-530 | The MEM.UPLOAD err fix exits S_ISSUE, but the shown diff does not show the request wire being deasserted. | It sets `st_q <= S_RETIRE`; no shown assignment clears `hps_req`. |

## 1. Pending capture for HOLDER and PULSER

For a **PULSER**, the capture is correct for the one-cycle style shown in the contract:

- If the pulse arrives while another client owns the bridge, `state != A_IDLE` and `owner != i`, so the capture condition is true and `pend_v[i]`/`pend_req[i]` are set: fpga/rtl/memory/zhao_hps_arbiter.sv:295-301.
- If the pulse arrives in `A_IDLE` and that client wins the pick, it is served directly from the live request, not captured, because the capture loop excludes the picked client: fpga/rtl/memory/zhao_hps_arbiter.sv:297,306-312.
- If the pulse arrives in `A_IDLE` and loses to a lower index, it is captured because `pick != i`: fpga/rtl/memory/zhao_hps_arbiter.sv:297.
- Later, `eff_v`/`eff_req` make the pending request indistinguishable from a live one for the pick and for the bridge pulse: fpga/rtl/memory/zhao_hps_arbiter.sv:193-198,204-213,306-312.

For a **HOLDER**, the capture is also correct for the holding style:

- While another client owns the bridge, the holder’s live request is captured: fpga/rtl/memory/zhao_hps_arbiter.sv:295-301.
- While it is the owner, it is excluded from capture: fpga/rtl/memory/zhao_hps_arbiter.sv:298.
- When it is picked, its pending bit is cleared on the same edge: fpga/rtl/memory/zhao_hps_arbiter.sv:311.

A holder is **not served twice from pending and from its live request in the compliant case**. The pending copy is cleared when selected: fpga/rtl/memory/zhao_hps_arbiter.sv:311. While the client is the owner, capture is excluded: fpga/rtl/memory/zhao_hps_arbiter.sv:298. If the holder follows the stated contract and leaves its request state on grant or `err`, there is no second service of the same request.

However, if a holder keeps `req_i[i].valid` asserted after grant, the arbiter will treat the still-held request as a new request after the burst completes and can serve it again: fpga/rtl/memory/zhao_hps_arbiter.sv:296-300,306-312. That is finding #2. The arbiter has no way to distinguish “same request still held” from “new request” except through the client’s deassertion.

## 2. Priority and wait counters

Priority is preserved. The pick is made over `eff_v`, not just live `req_i`:

- `eff_v[i] = pend_v[i] || req_i[i].valid`: fpga/rtl/memory/zhao_hps_arbiter.sv:195.
- The pick loop runs downward and assigns `pick` for every effective requester, so the final assignment is the lowest index: fpga/rtl/memory/zhao_hps_arbiter.sv:204-213.

So a lower-index live request beats a higher-index pending request, and a lower-index pending request beats a higher-index live request. A pending request does not jump over a lower index; it only preserves the request after the original cycle.

The wait counters are also updated correctly for the stated purpose:

- Counters are updated for `i = 1` to `N-1` only, matching the port width `wait_cycles_o[N-1:1]`: fpga/rtl/memory/zhao_hps_arbiter.sv:163,286-290.
- The condition uses `eff_v[i]`, so a pulsing client whose live `valid` has dropped but whose pending slot is set continues counting: fpga/rtl/memory/zhao_hps_arbiter.sv:286-290.
- It stops counting when the client is the owner in a non-idle state: fpga/rtl/memory/zhao_hps_arbiter.sv:287.
- It does not double-count a client that has both pending and live request, because `eff_v[i]` is one boolean: fpga/rtl/memory/zhao_hps_arbiter.sv:195,286-290.
- It counts the `A_IDLE` cycle on which the client is picked, because `state` is still `A_IDLE` until the next edge. That is a reasonable definition of “waiting”: the client does not own the bridge until the next state.

The old bug described in the commit, where a pulser’s wait read one cycle and froze, is addressed by using `eff_v` instead of live `valid` in the wait loop: fpga/rtl/memory/zhao_hps_arbiter.sv:286-290, with the old line shown in the diff as `req_i[i].valid`.

## 3. Stale pending requests

Yes, a pending slot can hold a stale request.

- Once `pend_v[i]` is set, it remains set until reset or until that client is picked: fpga/rtl/memory/zhao_hps_arbiter.sv:273,311.
- While pending, `eff_req[i]` selects the stored `pend_req[i]`, not the current live request: fpga/rtl/memory/zhao_hps_arbiter.sv:196.
- If the client later changes its live request, the capture loop will not update `pend_req[i]` because it requires `!pend_v[i]`: fpga/rtl/memory/zhao_hps_arbiter.sv:296-300.

This is the explicitly stated cost in the header: a request, once offered, will be served, and a client may not withdraw an offered request: fpga/rtl/memory/zhao_hps_arbiter.sv:93-97. For compliant clients, this is intended. For a client that legitimately withdraws or changes its request before service, the arbiter will serve the old captured request and suppress the newer live request. That is finding #1.

## 4. `err` fixes in MEM.UPLOAD and DEBUG.FRAMEBLIT

**MEM.UPLOAD**: The shown fix does leave the request-issuing state cleanly on a no-grant refusal, at least at the state-machine level shown.

- In `S_ISSUE`, it now checks `hps_rsp_i.err` before `hps_req_grant_i`: fpga/rtl/mem/zhao_mem_upload.sv:527-530.
- On `err`, it sets `hps_err_q` and transitions to `S_RETIRE`: fpga/rtl/mem/zhao_mem_upload.sv:528-529.
- Therefore it no longer remains in `S_ISSUE` waiting for a grant that will not come on a malformed-refusal `err`.

However, the diff does not show how the actual `hps_req` wire is deasserted in `S_RETIRE`. It shows the state exit, not the request-generation logic. So “leaves request state cleanly” is proven only to the extent that the state exits `S_ISSUE`. That is finding #5.

**DEBUG.FRAMEBLIT**: not shown. The provided inputs include the MEM.UPLOAD diff, but not the DEBUG.FRAMEBLIT diff. I cannot verify whether it now leaves its request state on `err` or whether it can still spin.

Can either still spin? From the shown MEM.UPLOAD code, the specific `S_ISSUE` no-grant `err` spin is removed. Other states are not shown. For DEBUG.FRAMEBLIT, not shown.

## 5. Directed cases against old RTL

Cases **8** and **8b** fail the old RTL for the right reason, based on the shown diff:

- The old pick used live `req_i[i].valid` only; the new code uses `eff_v[i]`: fpga/rtl/memory/zhao_hps_arbiter.sv:208, with the old line shown in the diff as `if (req_i[i].valid)`.
- The old wait counter used live `req_i[i].valid` only; the new code uses `eff_v[i]`: fpga/rtl/memory/zhao_hps_arbiter.sv:286, with the old line shown in the diff.
- Old RTL had no `pend_v`/`pend_req` capture. A one-cycle pulse while another client owns the bridge would be invisible after that cycle, producing zero beats. Case 8 checks that the pulsing client gets 8 beats: tests/memory/hps_arbiter_n_directed.cpp:525-527.
- Case 8b covers the pulse that loses an `A_IDLE` arbitration to a lower index. New RTL captures it because `state == A_IDLE`, `any_req` is true, and `pick != i`: fpga/rtl/memory/zhao_hps_arbiter.sv:297. The test checks that the pulse is served after the winner: tests/memory/hps_arbiter_n_directed.cpp:551-552.
- The `who == 2` wait counter check is also targeting the old freeze: old live-only counting would count the one pulse cycle and then stop; new `eff_v` counting continues while pending: fpga/rtl/memory/zhao_hps_arbiter.sv:286-290; test check at tests/memory/hps_arbiter_n_directed.cpp:538-539.

Case **9** is different. It does not fail the old RTL for the pending bug:

- The RTL A_ACTIVE write-beat mux is unchanged: fpga/rtl/memory/zhao_hps_arbiter.sv:238.
- The commit only changed the surrounding comment: fpga/rtl/memory/zhao_hps_arbiter.sv:234-237.
- The test checks that no write beat reaches HPS during a read burst: tests/memory/hps_arbiter_n_directed.cpp:569-570.
- The bench’s `hps_write_beats` counter is declared in the shown diff, but its increment logic is not shown: tests/memory/hps_arbiter_n_directed.cpp:98. Therefore the exact meaning of “reaches the HPS” in the bench is not fully shown. The RTL comment says the bridge takes a write beat only while `busy_write`: fpga/rtl/memory/zhao_hps_arbiter.sv:235-237.

So: 8 and 8b are the old-RTL failing cases for the pending fix; 9 is a pin/regression check, not an old-RTL failing case for this bug.

## CONTINUATION
- Done: Pending capture serves one-cycle pulses and held requests when compliant: `zhao_hps_arbiter.sv:193-198,295-311`; lower-index priority uses `eff_v` at `204-213`; wait counters use `eff_v` at `286-290`. Stale pending is possible: `196,296-300,311`. MEM.UPLOAD retires on no-grant err: `zhao_mem_upload.sv:527-530`. Cases 8/8b target old lost pulse (old pick/wait at current `208,286` per diff); case 9 is a pin, not an old-fail case: test `569`, RTL `238`.
- Open: DEBUG.FRAMEBLIT fix not shown. MEM.UPLOAD request deassertion in S_RETIRE not shown. Whether bridge can err in A_PULSE is not shown; A_PULSE ignores err at `316-319`. Bench `hps_write_beats` counting and bridge write-beat acceptance not shown. Holder duplicate depends on client deassert-on-grant code not shown.
- Next chunk: Review the DEBUG.FRAMEBLIT diff from 525a3f6d and any holder client request state, to verify err/grant leave request state cleanly. Inputs: commit diff for DEBUG.FRAMEBLIT file, MEM.UPLOAD request-drive code, and `zhao_hps_arbiter.sv:321-333` grant/rsp timing.

## Coordinator verdict

**partial** — spot 2: F1 CONFIRMED as written (capture needs !pend_v at arbiter:296-300, so a client that changes its request while its slot is occupied is SILENTLY dropped) but it is unreachable for a PULSER that pulses once and waits; the real defect is that nothing COUNTS the drop. F2 is a pre-existing risk, not a regression: the capture is correctly blocked while the client is the owner (:299), so a duplicate needs a holder that is still asserting after its burst ENDS -- true of the old RTL too. F3 (stale rule-number comment) and F4 (case 9 is a pin, not an old-fail case) are correct and LOW. F5 is an input limit (the deassert is outside the shown diff). No false claims. -> R55.
