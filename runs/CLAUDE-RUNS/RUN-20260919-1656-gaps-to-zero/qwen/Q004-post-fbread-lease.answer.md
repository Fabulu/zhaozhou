# Q004 answer — post-fbread-lease

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: debae03be6e947f6918366d6612cf6a7
- when: 2026-09-19T22:20:23  seconds: 763  finish: stop  status: ok
- usage: {"completion_tokens": 44725, "completion_tokens_details": {"reasoning_tokens": 42260}, "prompt_tokens": 16160, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 60885}
- inputs: [{"input": "fpga/rtl/compositor/zhao_post_fbread.sv:1-331", "sha256": "14b78dbdfd30aa16", "chars": 16804}, {"input": "fpga/rtl/compositor/zhao_post_lease.sv:1-485", "sha256": "7895fcc83ffdebed", "chars": 25712}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P1 | fpga/rtl/compositor/zhao_post_lease.sv:204 | `view_o` is not the view opened by `pass_start_o`; the second Duo pass starts with view 0, and the first pass after a Duo frame can start with stale view 1. | `view_o = view_q` (204), but `view_q` changes only after the start cycle (448, 458); `next_view_c` is used only for echo (262). |
| 2 | P2 | fpga/rtl/compositor/zhao_post_lease.sv:473 | `frame_admit_i` can drop the sequencer to `S_IDLE` without aborting/masking the reader or echo, so an in-progress post pass can keep ENGINE0 active while the shell returns FBWRITE to the raster. | Unconditional `if (frame_admit_i) st_q <= S_IDLE;` (473) does not check `rd_busy`/`echo_busy` or gate the share (307) / reader / echo requests. |
| 3 | P2 | fpga/rtl/compositor/zhao_post_fbread.sv:239 | `owed_q` can keep a phantom beat if the first beat arrives in the same cycle as `ok` (or before `ok`) when `owed_q` was 0, reducing `room_c` and possibly stalling the pass. | Add is on `(r_st_q == R_VERD) && guard_rsp_i.ok` (238), but subtract is only `beat_valid_i && (owed_q != '0)` (239). |
| 4 | P2 | fpga/rtl/compositor/zhao_post_lease.sv:402-407 | The retire FIFO has no full guard; if more than 4 passed ENGINE0 requests await credits, `rq_wp_q` wraps over live entries and attribution can corrupt. | `RQ = 4` (341); `push_c` increments `rq_wp_q`/`rq_n_q` with no full test (402-407); reader bounds beats, not request count, via `room_c` (zhao_post_fbread.sv:140). |
| 5 | P3 | fpga/rtl/compositor/zhao_post_fbread.sv:231 | `overflow_o` is a dead tripwire under the shown credit law: a beat with no queue room cannot arise from the request gating. | `room_c` gates requests by `FIFO_BEATS - q_count_q - owed_q` (140, 290); overflow only checks `beat_valid_i && q_count_q == FIFO_BEATS` (231). |

## QUESTION ANSWERS

### 1. Can the lease grant ENGINE0 to post before the raster has drained?

From the shown start gating, **no**:

- `start_c` in `S_ARMED` requires `raster_done_c` and `lease_live_i` (`zhao_post_lease.sv:199-201`).
- `raster_done_c` requires `fbw_drained_i`, i.e. every raster FBWRITE word retired (`zhao_post_lease.sv:194-195`).
- The source reader starts only on that `S_ARMED` start: `start_i = start_c && (st_q == S_ARMED)` (`zhao_post_lease.sv:222`).
- Post write-back output is only enabled in `S_PASS` (`zhao_post_lease.sv:240-241`), which is entered after that start (`zhao_post_lease.sv:446-449`).

So a post pass cannot **start** while raster writes are in flight.

However, the shown code does **not** protect an already-running pass from `frame_admit_i`. If `frame_admit_i` asserts during `S_PASS` or `S_SETTLE`, the sequencer goes to `S_IDLE` (`zhao_post_lease.sv:473`) without aborting the reader/echo or masking the share. Whether the shell guarantees `frame_admit_i` cannot happen before the post pass is fully retired is **not shown**.

### 2. Can a response beat be mis-steered to another requester?

From the shown files, **not established**.

- The lease routes read beats to the reader using the share’s per-requester valid and shared beat data: `rd_beat_v = sh_bv[1]`, `rd_beat_d = sh_bd` (`zhao_post_lease.sv:325-326`).
- Whether `sh_bv` is one-hot, whether the share tags beats, and whether two requests can overlap safely are properties of `zhao_mem_share_n` and the guard, which are **not shown**.

The reader itself does allow a new request while previous beats are still owed: `room_c` only requires room for the new request’s beats, counting `owed_q` (`zhao_post_fbread.sv:140`, `zhao_post_fbread.sv:239`, `zhao_post_fbread.sv:290`). So a request can be issued while a previous response is still streaming; whether the share/guard support that is **not shown**.

The P1 view bug is a view mis-steer, not a beat mis-steer.

### 3. Does `zhao_post_fbread` read exactly the frame?

For valid geometry, the reader’s own walk is correct:

- Request chunk is `min(32, w - rx)` (`zhao_post_fbread.sv:129-130`).
- Beats per request is `npx / 4` (`zhao_post_fbread.sv:132-133`).
- Byte length is `npx * 2` and `be` covers exactly those bytes (`zhao_post_fbread.sv:150-153`).
- A row advances by `stride_i` only after the row’s last request; the last row does not advance (`zhao_post_fbread.sv:304-310`).
- Pixel order within a beat is low address to high address: pixel k is `beat[16*k +: 16]` (`zhao_post_fbread.sv:178`).
- Bad width/zero geometry is refused at start (`zhao_post_fbread.sv:193-194`, `zhao_post_fbread.sv:276-285`).

For Duo, the lease gives the reader one tall pass with `h = 2 * frame_h` (`zhao_post_lease.sv:216`) and the slot base as origin (`zhao_post_lease.sv:223`), so it reads the combined stored surface in raster order.

Caveat: the shown reader has no explicit VRAM address overflow check for `origin + ry*stride + x*2`; whether overflow is possible depends on `ZHAO_VRAM_ADDR_BITS`, `origin_i`, `stride_i`, and mode bounds, which are **not shown**.

### 4. Backpressure: lost or duplicated beats if the consumer stalls mid-row?

Under the shown credit law, **no beat is duplicated**, and no beat should be lost:

- Pop/head-load is gated by `rd_en_c` (`zhao_post_fbread.sv:170`, `zhao_post_fbread.sv:233`).
- A beat is loaded once from `q_rp_q` and `q_rp_q` advances once (`zhao_post_fbread.sv:174`, `zhao_post_fbread.sv:233`).
- Consumer stall prevents `px_fire_c`, which prevents `head_done_c` and further pops (`zhao_post_fbread.sv:166-170`).
- New requests are gated by `room_c`, which accounts for queued beats and owed beats (`zhao_post_fbread.sv:140`, `zhao_post_fbread.sv:290`).

A beat could be lost only if the unreachable overflow path fired (`zhao_post_fbread.sv:231`) or an unshown guard/share behavior delivered an unaccounted beat. The phantom-`owed_q` issue can cause stall/deadlock, but it is not a loss/duplication path in the shown queue.

### 5. Any counter or guard that cannot fire?

Yes, one P3:

- `overflow_o` in the reader is structurally unreachable under the shown request gating, because `room_c` keeps `q_count_q + owed_q` within `FIFO_BEATS` before a request is issued (`zhao_post_fbread.sv:140`, `zhao_post_fbread.sv:231`).

The retire FIFO issue is the opposite: it is missing a full/overflow guard, not a guard that cannot fire.

## CONTINUATION

- Done: Established from shown RTL that post pass start is gated by `fbw_drained_i` (`zhao_post_lease.sv:194-201,222`) and post write-back only occurs in `S_PASS` (`zhao_post_lease.sv:240`). Found P1: `view_o` is stale on `pass_start_o`; second Duo pass starts with view 0, first pass after Duo can start with view 1 (`zhao_post_lease.sv:204,262,448,458`). Established reader address arithmetic is exact for valid `w/h` and Duo tall height (`zhao_post_fbread.sv:129-130,150-153,178,304-310`; `zhao_post_lease.sv:216,223`). Backpressure has no loss/dup under the credit law (`zhao_post_fbread.sv:140,167-170,233,290`). `overflow_o` is dead by design (`zhao_post_fbread.sv:140,231`).
- Open: Need share/guard contract: `sh_bv` one-hot/beat tagging, max outstanding ENGINE0 requests, whether `ok` can coincide with `beat_valid_i`, and whether `frame_admit_i` is guaranteed not to assert while `rd_busy`/`echo_busy` or before credits retire. Also need VRAM address width/mode bounds to close address overflow and retire FIFO depth.
- Next chunk: Review `zhao_mem_share_n` and the guard response protocol to close Q2 and the P2 assumptions. Inputs: `zhao_mem_share_n.sv`, guard RTL/spec/tests, `zhao_pkg` client/beat/credit definitions, and the smoke/directed tests. Also confirm the compositor samples `view_o` on `pass_start_o` and fix it to `start_c ? next_view_c : view_q` if so.

## Coordinator verdict

**partial** — F1 mechanism CONFIRMED: view_o = view_q (lease:204) is stale on the start cycle while the echo already gets start_c ? next_view_c : view_q (lease:262). Severity likely over-rated: POST.COMPOSITE uses view_sel_i combinationally (composite:599,678), so a stale view lasts ONE cycle unless something latches the view at frame_start. The fix is one line and matches the echo's own form. F2 (frame_admit forces IDLE unconditionally, lease:473) CONFIRMED in source; its impact needs the shell's admit protocol. F3/F4 plausible, not yet checked. Queued ALL for post pass 2 with directed tests. No false claims.
