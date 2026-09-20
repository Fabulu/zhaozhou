# Q008 answer — hostregwin-noescape

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 11350915c14343cf9d9a48bb970cc170
- when: 2026-09-20T07:01:42  seconds: 544  finish: stop  status: ok
- usage: {"completion_tokens": 34581, "completion_tokens_details": {"reasoning_tokens": 31345}, "prompt_tokens": 13686, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 48267}
- inputs: [{"input": "fpga/rtl/debug/zhao_host_regwin.sv:1-320", "sha256": "8adc1563eeb23616", "chars": 16418}, {"input": "fpga/rtl/debug/zhao_host_reg_hist.sv:1-215", "sha256": "7ac301c42ab4801c", "chars": 10180}, {"input": "fpga/rtl/debug/zhao_host_reg_trace.sv:1-165", "sha256": "97c4537dd26c9c1f", "chars": 7923}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | fpga/rtl/debug/zhao_host_regwin.sv:284-294,298-310 | The timeout allows a tenant response on the cycle `ackc_q == ACK_LIMIT`, making the wait one cycle longer than the named `ACK_LIMIT` bound. | Ack/rvalid branches precede `else if (ackc_q == ACKW'(ACK_LIMIT))`, and `ackc_q` starts at 0 (266,285). |
| 2 | P2 | fpga/rtl/debug/zhao_host_regwin.sv:209-210,276-278; fpga/rtl/debug/zhao_host_reg_hist.sv:175-178; fpga/rtl/debug/zhao_host_reg_trace.sv:125-128 | A mapped write is presented to the tenant and acknowledged before the tenant returns err, so the aperture does not refuse it before presentation. | Mapped accesses go to `S_SEL` with `t_write_o = write_q`; both tenants set err only after `sel_i`. |
| 3 | P3 | fpga/rtl/debug/zhao_host_regwin.sv:163-169,249,258-259,269,273,288,301,304 | All evidence counters saturate, so events after `CNT_MAX` are not counted. | `sat_inc` returns `CNT_MAX` when the carry bit is set. |
| 4 | P3 | fpga/rtl/debug/zhao_host_regwin.sv:200-210; fpga/rtl/debug/zhao_host_reg_hist.sv:172; fpga/rtl/debug/zhao_host_reg_trace.sv:121 | Stale `t_woff_o/t_write_o/t_wdata_o` remain on the tenant bus when no tenant is selected; shown tenants gate on `sel_i`, so no state change. | `t_sel_o` is only generated in `S_SEL`, while the other three tenant outputs are unconditional assigns. |

## ANSWERS

### 1. Can a read be answered with another tenant’s data or another word?

No, from the shown RTL.

- `h_ready_o` is low except in `S_IDLE`: `fpga/rtl/debug/zhao_host_regwin.sv:226`.
- A request is accepted only in `S_IDLE`: `fpga/rtl/debug/zhao_host_regwin.sv:253`.
- Therefore a new address offered while an access is in flight is ignored by the aperture.
- `sel_q`, `woff_q`, `write_q`, and `wdata_q` are captured in `S_IDLE`: `fpga/rtl/debug/zhao_host_regwin.sv:261-264`.
- `sel_q` is not changed again until the next `S_IDLE` acceptance.
- Tenant response data is selected in `S_WAIT` using the stable `sel_q`: `fpga/rtl/debug/zhao_host_regwin.sv:221-224,298-302`.
- The host sees `h_rdata_o = rdata_q`, not live tenant data: `fpga/rtl/debug/zhao_host_regwin.sv:228`.

So there is no shown path by which a later offered address can change the tenant or word used to capture the in-flight response. The captured selector and captured response data are not loaded by the same enable in a way that lets a new request steal the response: selector/offset are captured in `S_IDLE`, data is captured in `S_WAIT`.

One external caveat: how the HPS bridge treats `h_valid_i` while `h_ready_o` is low is not shown. If the host bridge holds `h_valid_i` through a completed response, the aperture will accept it again in the next `S_IDLE`; that is a new accepted handshake per the shown module, but the external bridge behavior is not shown.

### 2. Can the aperture hang?

No hang is shown.

`h_ready_o` is low in exactly these states:

| state | meaning | exit path |
|---|---|---|
| `S_SEL` | offering access to tenant | ack to `S_WAIT`, or timeout to `S_RESP` |
| `S_WAIT` | tenant acked, waiting for data | rvalid to `S_RESP`, or timeout to `S_RESP` |
| `S_RESP` | one-cycle host response | `S_IDLE` next cycle |

Evidence:

- `h_ready_o = (st_q == S_IDLE)`: `fpga/rtl/debug/zhao_host_regwin.sv:226`.
- `S_SEL` exit: `fpga/rtl/debug/zhao_host_regwin.sv:283-295`.
- `S_WAIT` exit: `fpga/rtl/debug/zhao_host_regwin.sv:297-311`.
- `S_RESP` exit: `fpga/rtl/debug/zhao_host_regwin.sv:313-315`.

What lowers `h_ready_o`:

- `h_valid_i` in `S_IDLE` moves to `S_SEL` or `S_RESP`: `fpga/rtl/debug/zhao_host_regwin.sv:252-279`.

What raises it:

- Only reaching `S_IDLE` from `S_RESP`: `fpga/rtl/debug/zhao_host_regwin.sv:313-315`.

Specific stress cases:

- Abandoned request: the aperture does not require `h_valid_i` to remain high; it has already captured the request and will proceed.
- Tenant acknowledges then goes quiet: `S_WAIT` timeout: `fpga/rtl/debug/zhao_host_regwin.sv:303-307`.
- Tenant never acknowledges: `S_SEL` timeout: `fpga/rtl/debug/zhao_host_regwin.sv:287-291`.
- Timeout expires in the same cycle the tenant answers: the answer branch wins:
  - `S_SEL`: ack before timeout at `fpga/rtl/debug/zhao_host_regwin.sv:284-291`.
  - `S_WAIT`: rvalid before timeout at `fpga/rtl/debug/zhao_host_regwin.sv:298-307`.

No shown input sequence leaves `h_ready_o` low forever.

The only timing issue is Finding 1: the tenant can answer on the cycle where `ackc_q == ACK_LIMIT`, so the effective wait is one cycle longer than the named bound.

### 3. Can a write change tenant state?

No shown tenant measurement state can be changed by a write.

- Histogram tenant: a write only sets `err_q` and goes to `S_RESP`:
  `fpga/rtl/debug/zhao_host_reg_hist.sv:175-178`.
  It does not change `bin_q`, `rdata_q`, or any histogram input.
- Trace tenant: a write only sets `err_q` and goes to `S_RESP`:
  `fpga/rtl/debug/zhao_host_reg_trace.sv:125-128`.
  `armed_i` is only read in status word 0x200:
  `fpga/rtl/debug/zhao_host_reg_trace.sv:137`.
  There is no write path to `armed` in the shown tenant.

However, the aperture does not structurally refuse mapped writes before presenting them to the tenant. A mapped write goes to `S_SEL` and drives `t_write_o` and `t_wdata_o`:
`fpga/rtl/debug/zhao_host_regwin.sv:209-210,276-278`.
The tenants then refuse the write with `err_i`. That is Finding 2.

So:

- No shown tenant state change from writes.
- But “refused before anything is presented” is not true for mapped writes; the write is presented, acknowledged by the tenant FSM, and then refused by the tenant’s err response.

### 4. Is the timeout terminal value reachable and reset correctly?

Terminal value is reachable.

- `ACKW = $clog2(ACK_LIMIT + 1)`: `fpga/rtl/debug/zhao_host_regwin.sv:141`.
- For the shown default `ACK_LIMIT = 255`, `ACKW = 8`, so `ackc_q` can reach `255`.
- Timeout condition compares to `ACKW'(ACK_LIMIT)`:
  `fpga/rtl/debug/zhao_host_regwin.sv:287,303`.

Reset behavior:

- `ackc_q` is loaded to 0 in `S_IDLE` when a request is accepted:
  `fpga/rtl/debug/zhao_host_regwin.sv:266`.
- `ackc_q` is loaded to 0 when a tenant acknowledges in `S_SEL`:
  `fpga/rtl/debug/zhao_host_regwin.sv:285`.
- A successful `S_WAIT` exit does not explicitly reset `ackc_q`, but the next accepted request in `S_IDLE` resets it before entering `S_SEL` again:
  `fpga/rtl/debug/zhao_host_regwin.sv:261-266,276-278`.

Therefore a second slow read does not immediately time out because of a stale `ackc_q`, as long as the next request is accepted in `S_IDLE`.

The timing defect is Finding 1: the tenant can assert ack/rvalid on the cycle where `ackc_q == ACK_LIMIT`, so the code accepts a response one cycle after the counter reaches the named limit.

### 5. Are counters incremented exactly once per event?

From the shown RTL, no double-counting is shown for the normal events.

| counter | increment site | event |
|---|---|---|
| `reads_o` / `writes_o` | `fpga/rtl/debug/zhao_host_regwin.sv:258-259` | accepted `h_valid_i` in `S_IDLE` |
| `refused_misaligned_o` | `fpga/rtl/debug/zhao_host_regwin.sv:269` | misaligned accepted request |
| `refused_unmapped_o` | `fpga/rtl/debug/zhao_host_regwin.sv:273` | unmapped accepted request |
| `refused_tenant_o` | `fpga/rtl/debug/zhao_host_regwin.sv:301` | tenant `rvalid_i` + `err_i` in `S_WAIT` |
| `refused_timeout_o` | `fpga/rtl/debug/zhao_host_regwin.sv:288,304` | timeout in `S_SEL` or `S_WAIT` |
| `stall_cycles_o` | `fpga/rtl/debug/zhao_host_regwin.sv:249` | every non-`S_IDLE` cycle |

Notes:

- `refused_tenant_o` is only incremented when `sel_rvalid_c` is true in `S_WAIT`, and the state then moves to `S_RESP`, so it cannot pulse again for the same access.
- `refused_timeout_o` is only incremented in the timeout branch, which moves to `S_RESP`.
- If tenant rvalid and timeout occur in the same cycle, rvalid wins, so the access is counted as tenant response, not timeout:
  `fpga/rtl/debug/zhao_host_regwin.sv:298-307`.
- `stall_cycles_o` is a cumulative cycle counter, not a per-access pulse. It increments once per non-idle cycle.
- All counters saturate via `sat_inc`, so after `CNT_MAX` events they undercount: Finding 3.

## CONTINUATION
- Done: From the shown RTL, no cross-tenant or wrong-word read is possible; `h_ready_o` is low only in `S_SEL/S_WAIT/S_RESP` and each state has a finite exit (`zhao_host_regwin.sv:226,284-315`). Mapped writes do not change shown tenant data (`zhao_host_reg_hist.sv:175-178`; `zhao_host_reg_trace.sv:125-128,137`), but they are presented to the tenant before the tenant err refusal (`zhao_host_regwin.sv:209-210,276-278`). Timeout terminal value is reachable and `ackc_q` is reset in `S_IDLE` and on ack (`zhao_host_regwin.sv:141,266,285,287,303`), but ack/rvalid on `ackc_q == ACK_LIMIT` are accepted, making the wait one cycle longer than named (`zhao_host_regwin.sv:284-291,298-307`). Counters have no shown double-count, but all saturate (`zhao_host_regwin.sv:163-169`).
- Open: HPS bridge handling of `h_valid_i` when `h_ready_o` is low is not shown. `DEBUG.TRACE` one-cycle `rd_data_i` latency is not shown (`zhao_host_reg_trace.sv:107,149-153`). Histogram `rd_ready_i` / `rd_data_valid_i` behavior is not shown (`zhao_host_reg_hist.sv:194-206`); if `rd_ready_i` never returns, the hist tenant stays in `S_BIN` and later hist reads time out. Instantiation parameters `OFFW`, `TENANT_LSB`, and `ACK_LIMIT = 0` corner are not shown.
- Next chunk: Wire the three modules into a top or directed test and exercise: response at `ackc_q == ACK_LIMIT`, ack-then-quiet, held `h_valid_i` through `S_RESP`, timeout same cycle as rvalid, and write to trace armed word. Needed inputs: `zhao_debug_trace`, `zhao_measure_histogram`, the instantiation, and `tests/debug/host_regwin_directed.cpp`.

## Coordinator verdict

**verified** — All four findings are accurate statements about the RTL and NONE is a defect. F1 (timeout off-by-one) is real and is a PROSE error, not an RTL one: the ack test precedes the limit test deliberately, and Qwen missed that ackc_q is recleared on the ack so there are TWO budgets and the worst case is 2*(ACK_LIMIT+1); header corrected. F2 (a mapped write reaches the tenant before refusal) is exactly what the block's own header says it does, with a reason -- 'a third tenant that wants writes needs no change here'. F3 (counters saturate) is sat_inc, by design. F4 (stale tenant bus when unselected) is gated by sel_i in both tenants. No escape and no hang found, which is the claim I asked it to attack. Severities inflated as usual (two P2s for zero defects).
