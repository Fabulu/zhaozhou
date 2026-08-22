# SPEC v1: the saturating-counter law, the creature LOD ladder, and the CDC seam

**Run ID:** RUN-20260822-2136
**Created:** 2026-08-22 21:36 UTC+02:00
**Status:** Active
**Previous Version:** N/A

> **Coverage note.** This run folder was created at 21:36, late in a session that
> began around 04:00 the same day. It is backfilled to cover the whole session,
> because the run was not initialized at the start — the blunder that prompted
> making runs a permanent fixture. Commits covered: `995595f..HEAD`.

---

## Objective

Continue the hardware toward "all waves and phases", working the standing method
with no shortcuts: check the oracle resolves before any RTL, differential against
the SHIPPED oracle, mutation sweep with forced regeneration and a hash check,
close the gaps the sweep exposes, gates green, commit and push.

Success for this run:

- the timing change from the previous stretch MEASURED, not assumed;
- the owner's outstanding architecture decisions asked and acted on;
- at least one new block built to the full standard;
- nothing claimed as hardware-proven — everything here is simulation, synthesis
  or fit.

---

## Scope

**In Scope:**

- Apply the all-ones-minus-x identity across the saturating counters and prove
  the law that no simulation can reach.
- Measure the composed shell fit and report the delta honestly.
- Put the accumulated architecture questions to Fabian and implement the rulings.
- Build the creature representation ladder (`GEOM.MESHFETCH`'s LOD third).
- Clear step 1 (oracle resolution) for the next blocks in the queue.
- Delegate the GPU/video CDC seam to a subagent.

**Out of Scope:**

- Particle-simulation, compositor and 2D block behaviour — reserved to the owner
  by standing instruction.
- Chasing the remaining setup slack: the owner ruled the CDC seam comes first,
  because moving that logic changes placement.
- Any claim about a physical board. Nothing has run on hardware.

---

## Constraints

- One implementation subagent at a time.
- Commit and push logical commits during the run, not batched at the end.
- Everything is SIMULATION, synthesis or fit. Never imply hardware has run.
- Local gates must match CI.
- Do not invent game behaviour; put decisions on the owner docket instead.

---

## Don't Retry

*Failed approaches, so they are not re-learned after compaction.*

- **Do not trust a green test after a build you did not read.**
  `ninja: error: rebuilding 'build.ninja': subcommand failed` means ninja built
  NOTHING; ctest then reports "100% passed" against a stale tree. Caught twice
  this run — once by the test COUNT being 256 when it should have been 257.
- **Do not delete `build/.ninja_deps` to clear a file lock.** It costs ninja its
  dependency information and it silently relinks stale objects — 994 failures
  against provably clean RTL. Find and stop the lingering cmake process instead.
- **Do not stamp a mutated source's mtime into the future** to force a rebuild.
  A model elaborated from a MUTANT then outranks the pristine source restored
  after it, and elaboration is skipped.
- **Do not hash `V<top>.cpp` to decide whether a model re-elaborated.** That file
  is Verilator's wrapper and is byte-identical between pristine and mutant; the
  logic is in `V<top>___024root__0.cpp`. Hash the whole model directory.
- **Do not run `cmake -S . -B build` from a shell without
  `C:\Programmieren\dsstuff\mingw64\bin` first on PATH** — it poisons the cache
  with `CMAKE_CXX_COMPILER: C`.
- **Do not use msys2's `ctest`** — every test reports BAD_COMMAND. Use
  `C:/Programmieren/dsstuff/mingw64/bin/ctest.exe`.
- **Do not build a standalone Verilator probe to debug a model** — the libstdc++
  ABI does not match and it will not link. Add a diagnostic target inside the
  project's own cmake instead; it took one 40-second rebuild to find what two
  hours of theorising did not.
- **Do not write shell heredocs containing backticks or `\n`** — the Bash tool
  mangles both. Write the content with the Write tool to a scratch file and
  splice it in.
- **Do not start a comment with the word "Verilator"** in SystemVerilog —
  Verilator parses it as a pragma and fails the build.
- **Do not edit the shared working tree while a subagent is building in it.** It
  will absorb the in-progress files into its own commit.

---

## Open Questions

- The three earth-field WRITE ops (`FIELD.WRITE.MATERIAL/NAV/HAZARD`) still have
  no law; `TERRAIN.PATCH` sits downstream of them.
- The meshlet descriptor format for `GEOM.MESHFETCH` (where `bound_centre`
  lives) is undecided — a memory-layout question, not a missing law.
- Whether the 120 MHz fabric target binds the FPGA lane or the fabricated-silicon
  lane. On silicon it is a low bar; on this FPGA it is a redesign.
