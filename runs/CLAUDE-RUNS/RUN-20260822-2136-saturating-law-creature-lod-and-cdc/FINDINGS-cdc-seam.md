# Fix the GPU/video CDC seam - Findings

**Agent ID:** a69cac37667f114f8
**Created:** 2026-08-22 22:2x UTC+02:00
**Parent Task:** RUN-20260822-2136
**Status:** Complete

> **Filed by the parent, not the agent, and that is a defect in the process
> rather than in the work.** This agent was spawned before the run folder
> existed, so it was never told to write its own findings and wrote its record
> into the repository's root `TASK_LOG.md` instead (176 lines, commit
> `a62d35a`). The content below is its own reporting, preserved here so the run
> is complete. **Every future agent gets the FINDINGS template and the run path
> in its brief.**

---

## Summary

The displayed-CRC crossing was not a constraint problem, it was a structural one:
sixteen data bits plus `x`, `y` and `valid` were sampled by `gpu_clk` flops on
**every displayed pixel** — 122,880 per Duo frame — fenced by nothing but a phase
toggle. The CRC now runs in `vid_clk` and only the finalized 32-bit value crosses,
once per frame, on a toggle with data held stable. The displayed byte stream is
unchanged byte-for-byte, which is why the golden captures still match.

---

## Findings

### The seam was only ever "correct" because of the testbench

It worked because the simulation freezes `vid_clk = gpu_clk/2` with coincident
posedges — a property of the harness, not of a board. Both hold violations seen
in the HIGH PERFORMANCE fitter experiment were on that family, and a hold
violation is not a speed problem: **no clock is slow enough to fix data that
arrives too early.**

### Three documents disagreed, and the RTL followed the wrong one

`design/contracts/DEBUG.CRC.md` had always said "vid_clk domain for the
displayed-stream lane". `design/blocks.yml` said `clock_domain: gpu`. The RTL
followed the ledger and built the crossing. The owner ruled for the contract, so
`blocks.yml` is now `clock_domain: video` and all three agree. That change also
made the ledger's V8 correctly demand `async_bridge: true`, which the block now
declares — the rule had been silent only because the domain was mislabelled.

### A FIFTH way to score a run that never happened

The sweep's first run reported **0 caught, 22 survivors**. That is an impossible
number rather than a test hole, and chasing it found a guard the other four do
not cover:

> **The executable lives OUTSIDE the target directory.** `rm -rf <target>.dir`
> does not remove `build/tests/<target>.exe`, so a mutant that fails to COMPILE
> leaves the previous binary in place and the sweep runs that instead. A build
> failure then scores as a *caught mutant* — the most flattering possible way to
> be wrong.

What had failed to compile was the agent's own test file: `0x5EC0ND50u` is not a
valid hex literal. It had shipped in `eefc432` because **ctest does not build**,
and the directed lane had been reporting 54 checks where it should have reported
57 — three sof-restart checks had never executed at all.

The guard is now: delete the EXE too, and require it to exist after the build.
The parent session applied the same guard to `tools/sweep_geom_lod.sh`.

### One pixel is two bytes, so the fold does it in one tree

`zhao_debug_crc` takes a **pixel** port now. Two bytes fold in a single
`zhao_crc32c_fold` tree (~7 XOR levels) instead of two chained 8-level steps —
the same polynomial machine, held to the shipped `zhao_crc32c_step` by the
existing fold differential.

---

## Numbers

| lane | result |
| --- | --- |
| `debug_crc_directed` | 57 checks |
| `debug_crc_random` / `_nightly` | 2,100 checks / 3,000 frames green |
| `shell_golden_replay`, `shell_duo_markers_fast`, both lints | green |
| mutation sweep | **22 attempted, 22 accounted, 20 caught, 2 equivalent** |

Both survivors were declared EQUIVALENT *before* the run, each with a
reachability proof in the script footer (M21 the reset value of `crc_r`, M22 the
eof `n_bytes` clear — both dead by construction of the `running` gate), and both
are still driven every run.

**Step 1 was done before any RTL:** `zref::Crc32c` resolves
(`zref_cmd2.hpp:663`) and `zref::render::displayed_crc32c` resolves
(`zrender/resolve.cpp:97`). Neither is a phantom; neither was modified.

---

## Recommendations

- **Left for the owner, deliberately undecided** (now in `docs/OWNER_DOCKET.md`):
  1. **The SDC was not touched.** It keeps `gpu_clk` and `vid_clk` timed against
     each other specifically so this crossing stays visible. Cutting them now
     would improve the A/B by telling the tool to stop looking.
  2. **`starvation_o` is the last unstructured `vid_clk -> gpu_clk` path** — a
     64-bit counter sampled straight into a GPU register, guarded by a quiescence
     tripwire, which is a protocol argument rather than a structural one. If the
     two hold violations were on *that* family rather than the CRC, the remeasure
     will still show them. Options offered: measure now on the ruled change
     alone, or convert it to the same toggle handoff first.
- No fit was run. Everything above is simulation.

---

## Files Created in This Directory

None — this agent predated the run folder. Its artifacts landed in the repo:

- `tools/sweep_debug_crc.sh` - the mutation sweep, with the fifth guard.
- `TASK_LOG.md` (repo root) - 176 lines of its own record.

---

## Files Examined / Changed

- `fpga/rtl/debug/zhao_debug_crc.sv` - moved to `vid_clk`, pixel port, fold tree.
- `fpga/rtl/common/zhao_shell_top.sv` - glue 7; serializer removed.
- `design/blocks.yml` - `clock_domain: video`, `async_bridge: true`.
- `design/contracts/DEBUG.CRC.md` - the reading that was always correct.
- `tests/debug/debug_crc_directed.cpp` - the invalid hex literal, fixed.
