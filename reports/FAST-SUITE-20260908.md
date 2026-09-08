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

---

# CORRECTION: 6 failures, not 5 — and 3 tests never ran at all

The final ctest summary, which I did not have when I wrote the section above:

```
The following tests FAILED:
    4 - ledger_check          10 - golden_abi_info    11 - abi_golden
   12 - zcap_roundtrip        27 - format_check       30 - cppcheck_check
The following tests did NOT RUN:
  208 - texture_v3_window_identity
  212 - texture_v3rq_probe_sanity
  213 - raster_ticketq_rh_directed
```

Two things I got wrong by reading the partial log instead of waiting for the
summary:

**`cppcheck_check` FAILED.** I reported it as merely slow — it was advancing at
300+ CPU seconds, which was true, and I inferred it would pass, which was not
established. It ran to completion and failed.

**Three tests did not run**, and **all three are in my area**:
`texture_v3_window_identity`, `texture_v3rq_probe_sanity` and
`raster_ticketq_rh_directed` — the v3own window identity, the v3rq probe, and
the ticket queue.

Their executables **do not exist**. They are registered in `tests/CMakeLists.txt`
but were never built, because every build today targeted specific executables
and no full `cmake --build` ever ran. ctest reports a missing executable as
"Not Run" rather than a failure, which is why the pass/fail tally looked
complete.

## This weakens the claim I made

I wrote *"the texture work broke nothing — 449 pass around it."* That is still
true of the 449. But **three v3-related tests were never executed**, so the
statement covered less than it sounded like it did. A "Not Run" is not a pass,
and a tally that omits the category flatters itself.

Building and running them now. Until they report, the honest position is that
the texture work is verified by the 449 that ran and by the six texture gates,
and is **unverified against those three**.
