# RUN-20260818-0341 — ABI v3 completion, gallery cadence, shell characterization

Owner asks handled in this run: finish the red CI, slow the quadruped's legs,
make the moving-sun smears sun-sized, and get real Quartus evidence for the
composed shell. One agent policy observed: no subagents were used.

## State inherited

`main` was red at `e3f316b`. Three CI jobs failed:

| Job | Cause |
|---|---|
| npm tooling | root `package.json` listed a `compiler` workspace the lock file no longer had, so `npm ci` refused |
| format + static analysis | LLVM-15 formatting drift, then a cppcheck `objectIndex` finding |
| cmake + ctest (fast) | six C++ tests left behind by the ABI v2 -> v3 bump |

Also inherited: an uncommitted QSF hunk, and 18 directories of raw reel frames
at the repo root (~300 MB) which this run created by invoking `zhao-reel.exe`
with no output argument, where it defaults to `.`. Deleted; nothing tracked was
touched.

## Work

### `9f7a668` — hardware-only CI gates
Reverted the root manifest to the three tooling workspaces so `npm ci` matches
the lock, and applied pinned LLVM-15 formatting. npm lane green: 75 tests.

### `abf30b8` — four fitter processors
Quartus holds fit results invariant to processor count; the seed pins
reproducibility. Kept the inherited hunk deliberately rather than discarding it.

### `d8e7c88` — ABI v3 through the C++ lane
`5cebbcd` bumped the wire to v3 and QFMT to 2 on generator evidence alone. Six
tests were left behind, each wrong for its own reason:

- `stub_top` asserted v3 is a reject; it is now the accepted wire. Both reject
  cases now derive from `ZHAO_ABI_VERSION` so they cannot go stale again.
- `cmd_dma` built every frame header with a literal `2`, so a v3 decoder
  correctly refused all of them and every status collapsed to BAD_ABI_VERSION.
- `fixp` pinned QFMT 1. Amendment C1 added the quat16 lane rather than changing
  a law this file covers, so the checks are unchanged; quat16 is covered by
  `tests/geometry/creature_core.cpp`.
- `tables_tri` hardcoded the marker string; derived from the header now.
- `shell_golden`: the three 10-frame mode goldens were not regenerated with
  `duo_markers.zcap`. Verified at byte level before accepting: only the abi-gen
  provenance digest, `abi_version` `0x0002 -> 0x0003`, and the dependent
  header/section CRCs move. No scene drift.

### `5755d5c` — gallery cadence and smear scale
Quadruped: the 16-key stride ran four keys per frame while the body crept
0.022 m, so a full gait cycle covered 8.8 cm and a one-metre creature scrambled
in place. The clip clock is back on the sim clock at one tick per frame, so a
cycle is 32 frames and lays down 0.70 m, about one body length.

Suns: `ghost_r_px` was hand-authored into a 9..15 px band while the suns span
8..42 px, which is why every sun left the same footprint. Each subject now
smears at its own visible radius. Overlap costs no palette because the
reconstruction plane is six-bit: one class-ramp lookup yields at most 64 trail
colours however many falloffs a pixel sums. Every star sequence still clears
256; noctis-flare at 248 is the ceiling case.

### `4402bc5` — cppcheck objectIndex
`(&m.m00)[i]` walked off the first member of `ZhMat4fx`. The ABI
static_asserts pin the layout so it worked, but it is UB and cppcheck 2.13 on
the Linux runner is right to reject it. All sixteen members are named now. No
suppression was added.

## Evidence

- `reel --check`: all 18 sequence CRCs match
- fast CTest in `build-verify`: 86/87 observed passing in one run, plus
  `shell_golden_replay` verified directly at 756 checks after that run was
  killed part-way. Every fast test is therefore accounted for green.
- `cmd_dma_directed` 8894 checks, `cmd_dma_random` 1482, `stub_top` 46,
  `fixp` 29385050, `tables_tri` OK, `shell_golden` 756

## Interruptions

Three background jobs (Quartus elaboration, the fast CTest sweep, the GIF
encode) were killed externally part-way. Nothing was lost: CTest was resumed by
running its one outstanding test directly, and the GIF encode was restarted from
a clean state. The GIF set had been left half-new and half-a-day-old, which is
exactly the kind of mixed gallery that must never be deployed, so no deploy
happened until the encode completed in full.

## Maturity

No hardware maturity is claimed here. Quartus analysis/elaboration of
`zhao_shell_top` is running against the provisional `5CSEBA6U23I7` target. That
target is a capacity and timing characterization only. It is not board truth,
not a programmed device, and not integration. No `HARDWARE_PROVEN` claim is made
or implied.
