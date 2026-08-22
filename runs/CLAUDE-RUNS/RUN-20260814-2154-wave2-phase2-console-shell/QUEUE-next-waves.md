# QUEUE — remaining work after the parked-branch merges

*Orchestrator, 2026-08-16 08:05. Written while the video merge finishes, so the next dispatch is immediate. Serial discipline: one implementation agent at a time.*

## Order

### 1. W3.3 backend — **relaunch, not resume**
The survey found `wp/w3.3-backend` **0 commits ahead**: never started, despite earlier status calling it "barely started". Delete `.worktrees/w33`, relaunch from the wave-3 plan in `RUN-20260815-0544-wave3-phase3-software-console/`.

### 2. W2.7 — console shell + Duo marker demo (closes the Phase-2 gate)
Blocked until the video merge lands, since Duo scanout correctness is established there. Note the standing Duo facts so the agent does not re-derive them wrong:
- packed two-block layout: view0 `[0, 0x18000)`, view1 `[0x18000, 0x30000)`
- **slot allocation 0x3C000 vs occupancy 0x30000** — these differ deliberately
- `displayed_crc32c` **already includes** the 48 border rows. An earlier ratification of mine claimed it omitted them; **that was wrong** — only row *assembly* was faulty. Do not reintroduce the confusion.

### 3. Noctis star gamut + lens flares
Spec ratified in `RUN-20260815-2307-noctis-suns-and-flares/ADDENDUM-star-gamut-and-flares.md`; **unimplemented**. ZRef preview first, RTL at Phase 11. This is the highest-visibility item for the project owner — the suns are a stated must-keep ("beyond awesome") — and it feeds the render gallery and the Pages site.
Owner also requested **sun-streak-on-water**, which the world architect wrote as a request to the sky owner (Addendum §9) rather than editing the sky files. Pick it up here.

### 4. `REFRESH_URGENT` independent derivation
Per `RATIFICATION-refresh-urgent-justification.md`: derive from `tREF`/rows/policy, prove or disprove ≥18-cycle slack to `CNT_HARD`, correct the in-tree justification either way. Authorisation to re-pin the cycle-exact SDRAM tests and the zref oracle is granted **for that cause only**.

### 5. Terrain renderer + gallery at the new format
Now unblocked: the heightfield normal fix (`f7ad8d8`) and the dual-heightfield island format (`91006c2`). Owner's asks: **much bigger terrain, all polygons textured, floating islands with sky below.** Then regenerate `renders/` (ONE canonical image per subject, overwritten) and redeploy the Pages site.

### 6. QA + test-writer waves over wave 1
Specified in the owner's original loop (recon → architect → implement → review → QA → test-writer) and **never run for wave 1**. Given what the merges exposed — phantom oracles, vacuous assertions, never-run nightly lanes — wave 1's blocks deserve the same scrutiny before anyone trusts their badges.

## Standing constraints for every brief

- **PATH**: `.tools/oss-cad-suite` BEFORE `C:\programmieren\dsstuff\mingw64\bin`, or yosys dies `0xC0000139` before parsing.
- Export `VERILATOR_ROOT` at configure time (two targets fail on a clean tree otherwise).
- devkitPro cmake/ctest is broken; use winlibs.
- `ctest` counts SKIPs as passes and prints "100%" — state pass/skip/fail separately, never a percentage.
- **Free formal stimulus must enter as module input ports.** `read_slang` ties undriven locals to `1'x` and prep's opt folds those branches away *before* `setundef -anyseq` runs; `(* anyseq *)` on locals elaborates to constants. Both traps silently delete harness logic and produce passes against a folded model.
- **Every assertion needs a cover proving its precondition is reachable**, and check that harness bounds do not make the property unreachable (one assertion needed ≥153,600 B against a 64-B buffer and could never fire).
- **A by-construction claim in a header is an unverified assertion until something names its enforcer.** Three found false this session.
- **A citation is not evidence until a checker can resolve it** — V17 proposal (symbol/path existence checks) is written up and not yet implemented.
- Verilated exes must exit through `zhao::exit_hard` — `VlThreadPool::~VlThreadPool` deadlocks at exit on this toolchain.
- **Sweep for orphaned solver processes** after any killed/timed-out lane; an `sby` killed by CTest leaves its engine running and burning a core.
- Push intermittently, only on green you personally observed.

## Owner-facing state

Merged and pushed this session: MEM arbiter bound corrected + proven tight; ledger V16 (formal-run registry); world/terrain/creature/giant specs incl. the dual-heightfield rim decision; `wp/w2.6-cmd-debug` (CMD+DEBUG at UNIT_VERIFIED). In flight: `wp/w2.2-video`. Not started: items 1–6 above.
