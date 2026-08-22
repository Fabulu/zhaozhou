# RATIFICATION — `REFRESH_URGENT = 40` has lost its justification and must be re-derived independently

*Orchestrator decision, 2026-08-16. Raised by the arbiter-bound agent at `9d49806`, which correctly flagged rather than silently coupled.*

## What happened

`REFRESH_URGENT = 40` in `zhao_sdram_params_pkg` was justified in-tree as **"= the arbiter liveness bound"**.

That bound was disproved and re-derived (see `RATIFICATION-arbiter-liveness-bound.md`): refresh-free **52** (RR class) / **34** (scanout), operational **65 / 47** once the analytic 13-cycle refresh steal is added. So the sentence that justified `40` is now false.

The agent deliberately did **not** re-pin the constant to 65, because that moves `CNT_HARD` and would invalidate the cycle-exact SDRAM directed tests and the zref oracle — neither of which the arbiter ratification authorised. **That was the correct call** and is hereby endorsed: a constant must not be dragged along by a number it was only rhetorically attached to.

## The decision

1. **The coupling was never real, and is dissolved.** `REFRESH_URGENT` is not required to equal the arbiter liveness bound. They answer different questions:
   - the **liveness bound** answers *"how long may a client wait for a grant?"* — a fairness/latency property;
   - **`REFRESH_URGENT`** answers *"at what counter value must refresh stop being polite and start preempting?"* — a **data-retention** property, whose deadline comes from the SDRAM's `tREF` divided across its rows, not from anything the arbiter does.
   Setting one from the other was a rationalisation that happened to produce a survivable number. Two constants that look equal for unrelated reasons is exactly the shape of the error that produced `B = 40`.

2. **`REFRESH_URGENT` must be given its own derivation** from the part's `tREF`, row count, and the refresh scheduling policy, showing the slack between the urgent threshold and the hard deadline `CNT_HARD`. Until that derivation exists, the constant is **unjustified**, and the in-tree comment must say so in those words rather than citing the arbiter.

3. **The composition must be checked, not assumed.** The corrected liveness bound already charges a 13-cycle analytic refresh steal. That charge is only sound if an urgent refresh cannot itself be delayed past its deadline by a client burst that the arbiter refuses to preempt mid-burst. Required: show that `REFRESH_URGENT` leaves at least `MAX_BURST_SPAN` (18) of slack before `CNT_HARD`, or state the true relationship. **If 40 does not satisfy this, the number changes and the affected directed tests and the oracle are re-pinned deliberately** — with authorisation recorded here, which this document grants for that specific cause and no other.

4. **No maturity claim may cite `REFRESH_URGENT`'s value as justified** until (2) and (3) land.

## Why this is being written down instead of fixed inline

Three defects in a row on this lane shared one shape: **a number that looked right because something else made it look right** — a bound fitted to what passed, a proof that had never elaborated, a constant justified by a cross-reference nobody re-checked when the referent moved. The cheap fix each time (set it to 60; mark the test SKIP; bump 40 to 65) would have preserved the shape while changing the digits.

The discipline that caught all three is the same: *derive from the machine, then confirm — never adopt the number that passed.* Applying it here costs one more agent pass and is not optional.

## Queued as work

Assign to the next MEM-lane agent, after the world-architecture pass completes (serial execution is in force):

- derive `REFRESH_URGENT` from `tREF`/rows/policy, show the arithmetic
- prove or disprove the ≥18-cycle slack to `CNT_HARD`
- correct the in-tree justification comment either way
- if the value must change, re-pin the cycle-exact SDRAM directed tests and the zref oracle in the same commit, and say plainly in the message that the oracle moved and why

## Also carried forward from the same pass

- **`design/formal_runs.yml` + ledger rule V16** now make "never elaborated" a hard failure. It immediately caught `INPUT.SNAPSHOT`, which was `RTL_VERIFIED` on a property with **no cover task** — every assertion guarded by `$past(frame_tick.pulse)`, so an unreachable tick would have made all three vacuously true. Fixed with five covers. **Third block of the same disease; first one caught by machinery instead of by luck.**
- **The BMC horizon cannot see a refresh** (depth 130 vs 780-cycle interval). The harness now asserts its own scope (`a_horizon_is_refresh_free`) so raising the depth past the interval fires rather than silently changing what the numbers mean. This is the right pattern for every bounded proof in the repo and should be generalised.
- **Toolchain trap, repo-wide:** a mingw toolchain earlier on `PATH` than `.tools/oss-cad-suite` kills yosys with `0xC0000139` **before parsing** — "never elaborated" wearing a tooling costume. oss-cad-suite must come first for any formal work. Orchestrator briefs had this backwards and have been corrected.
- **Pre-existing, unrelated:** `VERILATOR_ROOT` unset at configure time breaks `test_empty_frame_replay` and `test_abi_fuzz_parity` on a clean tree.
