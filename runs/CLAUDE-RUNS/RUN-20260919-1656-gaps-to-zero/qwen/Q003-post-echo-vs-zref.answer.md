# Q003 answer — post-echo-vs-zref

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 6969f851e9a642f2b97fa9cacdb6d0a5
- when: 2026-09-19T22:08:59  seconds: 644  finish: stop  status: ok
- usage: {"completion_tokens": 45492, "completion_tokens_details": {"reasoning_tokens": 43604}, "prompt_tokens": 18951, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 64443}
- inputs: [{"input": "design/contracts/POST.ECHO.md:1-177", "sha256": "65aeb3e414ab9afe", "chars": 9963}, {"input": "fpga/rtl/compositor/zhao_post_echo.sv:1-314", "sha256": "6f1f120e4cd20ce7", "chars": 16724}, {"input": "reference/include/zref/zref_post.hpp:1-165", "sha256": "20215f716dc23ce0", "chars": 9666}, {"input": "tests/compositor/post_echo_directed.cpp:1-264", "sha256": "32ff3c50ac96d3dd", "chars": 12722}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | tests/compositor/post_echo_directed.cpp:162-259 | No test drives a geometry whose `views·h·w·2` exceeds the span, so the span branch (ref `geometry_ok` false / RTL open-then-guard-fault) is unreachable by the bench | largest case is Duo `2·192·256·2=196,608 < 0x3C000=245,760` (POST.ECHO.md:55-56, :110; zref_post.hpp:149) |
| 2 | P3 | design/contracts/POST.ECHO.md:140-147 | Contract's directed-test list ≠ shown test: "stalled beat" is in the console smoke, "no ready" is implicit, and the shown test adds an unlisted gappy-tap case | test case 4 (post_echo_directed.cpp:229-238) not in contract list; smoke file not shown |
| 3 | P3 | fpga/rtl/compositor/zhao_post_echo.sv:229 | `w==0` and `h==0` refusal branches never driven; only `w=100` (not a mult. of 16) is | `bad_geom_c` has 3 terms (sv:229); test only uses w=100 (post_echo_directed.cpp:244); POST.ECHO.md:109 |
| 4 | P3 | fpga/rtl/compositor/zhao_post_echo.sv:238 | "new `pass_start` over an open/drain pass → torn" (torn_inc_c term 2) never driven | bench sets `pass_start_i = first` only (post_echo_directed.cpp:65); a 2nd start over a live pass is absent |
| 5 | P3 | tests/compositor/post_echo_directed.cpp:251-259 | "every later pass TORN" after the fault latch is unverified — case 6 is last, no pass runs after the sticky fault | POST.ECHO.md:111; `fault_o` sticky (sv:222); nothing after case 6 |
| 6 | P3 | tests/compositor/post_echo_directed.cpp:96 | `pass_complete_o` pulse never directly checked; only the `passes_complete_o` counter is | POST.ECHO.md:117; test reads counter delta (cpp:96), pulse set jointly at sv:288-289 |
| 7 | P3 | design/contracts/POST.ECHO.md:124-125 | Contract's reference-function list omits `opens_chunk`, which the reference defines | zref_post.hpp:160 defines it; contract names only geometry_ok/capture_addr/chunk_of |
| 8 | P3 | tests/compositor/post_echo_directed.cpp:116 | Audit keys on `m.mem` (map by address), so a duplicate write to one address is collapsed and invisible; relies on unshown model violation counters | `m.mem.find(addr)` (cpp:116); backstop `bad_write_order/violations` (cpp:177), model not shown |
| 9 | P3 | design/contracts/POST.ECHO.md:47-49 | "32-byte bursts" granularity is not asserted by the shown test; the audit is per-pixel, delegating burst size to the unshown FBWRITE | audit loops per pixel (cpp:108-127); `zhao_raster_fbwrite` not shown |
| 10 | P3 | tests/compositor/post_echo_directed.cpp:158 | Test assumes ENGINE0 = client id 2; the ENGINE0→2 mapping is not in the shown material | `q.client == 2` (cpp:158); POST.ECHO.md:47,74 say "client ENGINE0"; zhao_pkg not shown |
| 11 | P3 | fpga/rtl/compositor/zhao_post_echo.sv:79 | `view_i` is 1 bit → at most 2 views; a ≥3-view geometry would truncate `view_q` and write into view-0 rows (within contract scope, which tops out at Duo) | `input logic view_i` (sv:79); POST.ECHO.md:55 (Duo = 2 views) |
| 12 | P3 | tests/compositor/post_echo_directed.cpp:69 | The exact sustained rate (16 px / ~22 clk) is not tested; the bench paces 16-on/7-off (16/23), a documented slower margin | POST.ECHO.md:95-97; `(cyc % 23) >= 16` (cpp:69), margin noted at cpp:47-50 |

**No P1 found.** The RTL implements the contract (address law, whole-16-px chunk drops decided at the first pixel, guard-based span refusal, no-ready, counters), and the reference's `geometry_ok`/`capture_addr`/`chunk_of` agree with it on every geometry the bench drives; the differential audit (per-pixel address + value + whole-chunk + no-stray) does catch the properties it exercises. I also checked the "dead guard/counter" question: no comparison has both operands behind one enable, and every counter (`pixels_written/dropped`, `passes_complete/torn`, `fault_o`, `pass_complete_o`) has a reachable firing path in the shown cases.

## CONTINUATION

- **Done:** RTL vs contract vs `zref::post::echo` agree on the tested geometries. Verified: address law (`crow_c` sv:163 → `fbw_y` sv:181 → `fb_stride_i={w_q,1'b0}` sv:196 = `capture_addr` zref:154); chunk admission is whole-16-px, decided at `tap_x_i[3:0]==0` with `room_c` sv:161 held via `keep_q` sv:275, and the skid provably can't overflow (count ≤ SKID); `settle_c` sv:232 = all-handed + `fbw_drained`; torn summed once via `torn_inc_c` sv:236-239 (at most 2 coincide, 2-bit safe); `fault_o = fbw_fatal||fbw_stream_err` sv:222; no dead counters/guards. Test checks: per-pixel addr+value, `partial==0`, `dropped==16·absent`, `written==present`, `strays==0`, complete/torn/fault deltas (cpp:108-138, 162-259).
- **Open (not shown, can't verify):** `zhao_raster_fbwrite` (does it really flush 16-px bursts and tolerate 16-px gaps without `stream_err`? — the starving case's `!o.fault` cpp:226 is the only evidence); `post_mem_model.hpp`/`zhao_sim.hpp` (do `bad_write_order`/`violations` catch duplicate writes and ordering faults?); ENGINE0→client-2 mapping; the console smoke `tb_zhao_console_core_smoke.sv` (the contract's "stalled beat written once" and word-for-word capture check live there, not in the shown bench).
- **Next chunk:** Add a span-exceeding directed case (e.g. w=512,h=256,views=2 → 524,288 > 245,760) that asserts `fault_o==1` and `torn==1` (the open-then-guard-fault path, POST.ECHO.md:110), plus small cases for `w==0`, `h==0`, a `pass_start` over a live pass, and a post-fault pass. Needs: the bench's `pass()` (cpp:51), the memory model's `allow` hook, and confirmation from `zhao_raster_fbwrite` that an out-of-window burst latches `fbw_fatal` rather than `fbw_stream_err`.

## Coordinator verdict

**verified** — spot 3/3: bad_geom_c has exactly the three terms (echo.sv:229) and only w=100 is driven; the largest tested geometry is 384x240x1 = 184,320 B, so the span-overflow row (contract :110) is never exercised; pass_start_i is driven only by the first pass. 12 findings, 0 P1, and no false claim found. The findings are real COVERAGE holes, queued for post pass 2 (span overflow, w/h==0, pass_start over a live pass, a pass after a fault).
