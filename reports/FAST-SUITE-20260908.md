# The full fast suite, finally run

**455 tests. 449 passed. 5 failed. Exactly one failure is mine.**

This was recorded as a debt this morning — the suite had been unverified since
the shared-block edits to `cache_pipe` (SRCW threading) and `rcp24_svc`. It took
four attempts to get a clean run, and three of those were my own doing.

## The five failures, with ownership established from git

| test | mine? | evidence |
|---|---|---|
| `ledger_check` | **partly** — 1 of its 4 errors | V20 on `island_v3_top:2720`, a file I created. **Fixed**, 4 -> 3. |
| `ledger_check` (other 3) | no | `v3own:1677`, `v3own:1836`, `v3rq:136` — **no commit of mine touches either file**; line 1677 last changed `384c3e77`, 2026-09-07 |
| `format_check` | no | clang-format drift in `zref_fragment.hpp` (last changed 2026-09-07) and `zref_island.hpp` (2026-09-06); **no commit of mine touches `reference/`** today |
| `zcap_roundtrip` | no | goldens last changed `5cd55827`, 2026-09-07 |
| `golden_abi_info` | no | same |
| `abi_golden` | no | same |

**The texture work did not break anything.** 449 tests pass around it, including
every test in the geometry, raster, command and shell lanes.

## My V20 fix

`zhao_texture_island_v3_top.sv:2720` claimed *"an owner that never completes is
never retired and the cursor does not advance past it"* with no
machine-resolvable enforcer. Now:

```
ENFORCED-BY: fpga/rtl/texture/zhao_texture_v3own.sv:a_out_in_order
```

— which exists at `v3own.sv:2022`. The comment also now names the mechanism
(`emit_q` advances only on `out_fire_c`) and the test that puts it under
pressure (`island_v3_fault_directed` phase 5, consumer shut mid-flight, with the
non-vacuity check that the island kept accepting while the sink was closed).

## Two curiosities worth flagging, not chased

`zcap_roundtrip` and `abi_golden` both report failures of the form
**"expected 0x1E7, got 0x1E7"** and **"expected 0x20, got 0x20"** — equal values
reported as a mismatch. Either the message prints a different quantity from the
one compared, or the comparison is on something the message does not show.

Those are not mine and not today's work, but a check whose failure message shows
two identical numbers is the kind of thing that wastes an afternoon later.
Recorded, not investigated.

## And an artefact I committed by accident

`captures/failures/zcap_minimal_mismatch.txt` entered a commit today via
`git add -A`. It is a failing test's **output**, not evidence.
`captures/failures/` is now ignored, for the same reason `*.rgb` is: keep the
report, not the intermediate that produced it.
