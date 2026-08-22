# QUEUE — merging the parked Wave-2 branches (survey + brief)

*Orchestrator survey, 2026-08-16 02:35. Read-only; no merge performed. Serial discipline in force — the world-architecture agent owns the main working tree right now.*

## Survey result

| branch | ahead | **behind main** | payload |
|---|---|---|---|
| `wp/w2.2-video` | 2 | **65** | VIDEO RTL (mode/scanout×3/scaler/framectl) + `zref_video` oracle — 2,468 insertions |
| `wp/w2.6-cmd-debug` | 4 | **32** | CMD+DEBUG zref, RTL, directed+random tests, **2 `.sby` formal tasks**, ledger promotions to `UNIT_VERIFIED` — 4,594 insertions |
| `wp/w3.3-backend` | **0** | 24 | **nothing.** Never started, not "barely started". |

Also parked and already merged-or-superseded: `wp/w2.1-spec`, `wp/w2.5-mem`, `wp/w3.5-renderer`.

## Three findings that change how these get merged

### 1. `wp/w3.3-backend` is empty — relaunch, do not merge
Zero commits ahead. The worktree at `.worktrees/w33` has no work in it. Earlier status calling this "barely started" was wrong; it is unstarted. Delete the worktree and relaunch W3.3 from the wave-3 plan.

### 2. Both branches predate ledger rule V16 and **should fail on contact**
`wp/w2.6-cmd-debug` ships `tests/formal/cmd_dma_crc_gate.sby` and `tests/formal/cmd_scheduler_slot_fsm.sby`, and promotes `CMD.SCHEDULER`, `CMD.DMA`, `DEBUG.COUNTERS`, `DEBUG.CRC` to `UNIT_VERIFIED`.

V16 (landed `d997ced`) requires every `.sby` on disk to have a recorded run in `design/formal_runs.yml`, and any block at ≥`RTL_VERIFIED` citing `tests.formal` to have `status: green` **and** `covers: true`.

**Expect `ledger:check` to fail after the merge. That is the correct outcome, not an obstacle.** The merge agent must make those proofs actually elaborate and record green runs *with covers* — never mark them skipped, never relax V16 to get the merge through. V16 has already caught three blocks carrying badges on proofs nobody ran (`MEM.GUARD`, `MEM.VRAM.ARBITER`, `INPUT.SNAPSHOT`); this is the fourth contact and the first one where the machinery gets to work *before* the badge lands rather than after.

Note the promotions here are only to `UNIT_VERIFIED`, which V16's ≥`RTL_VERIFIED` clause does not gate — but the "every `.sby` needs a recorded run" clause applies regardless. Do not use the maturity level as an excuse to leave the proofs unrun; they were written to be run.

### 3. Staleness is the actual risk, and it is concentrated in the video oracle
`wp/w2.2-video` is **65 commits behind**. Landing between then and now, among others:

- the raster crack pair (`cc98d94`) — pixel-centre bbox `(v_min+127)>>8`, and top-left bias applied to the exact s64 edge value `E0` rather than the floored `E'`
- the fill rule `≥` amendment (`57f1639`)
- `unit8_from_fx16` wrap at 0.998–0.9999 (`2237766`)
- `fx_sin` reading one past the 257-entry table (`4399420`)
- the heightfield normal quantisation fix (`f7ad8d8`)
- the Duo packed two-block layout ratification and the sky drum fix (`ae3bdf2`)

`zref_video.cpp` (798 lines) and `zref_video.hpp` (343) were written against the *pre-fix* numeric law. **A clean textual merge is the dangerous case here**, because git will report no conflict while leaving an oracle that disagrees with the corrected one. Merging these is a semantic review, not a textual one.

**Required of the merge agent:** after merging, re-derive the video oracle's outputs against current `spec/qformats.md` and `spec/video_rules.md` and prove agreement — do not assume a conflict-free merge means a correct merge. Specifically re-check anything touching pixel coverage, fx16→unit8 conversion, `fx_sin`/`fx_cos`, and the Duo layout (`view0 [0,0x18000)`, `view1 [0x18000,0x30000)`; slot allocation 0x3C000 vs occupancy 0x30000).

## Order of work

1. **`wp/w2.6-cmd-debug` first** — less stale (32), self-contained (CMD/DEBUG), and its formal tasks exercise V16 on a branch where the promotions are still modest. Lower blast radius, and it proves the V16 path on a real merge.
2. **`wp/w2.2-video` second** — 65 behind and semantically entangled with every renderer fix above. Give it its own agent and its own review.
3. **Relaunch W3.3 backend** from the wave-3 plan; remove the empty `.worktrees/w33`.
4. Then **W2.7** (shell + Duo marker demo) to close the Phase-2 gate.

## Standing constraints for whoever takes these

- One implementation agent at a time. These merges touch the main working tree; do not run them concurrently with each other or with a spec agent.
- `PATH`: `.tools/oss-cad-suite` **before** `C:\programmieren\dsstuff\mingw64\bin`, or yosys dies `0xC0000139` before parsing. Export `VERILATOR_ROOT` at configure time or two test targets fail to compile on a clean tree.
- devkitPro cmake/ctest is broken (mangles test paths) — use winlibs.
- `ctest` counts SKIPs as passes. State pass/skip/fail separately; never quote "100%".
- Commit in logical chunks and push to `origin/main` as you go.
