# OWNER DIRECTION — TEXTURE FIRST, TERRAIN DEFERRED

**Received and acknowledged 2026-09-07** by the implementing session.

> *"Finish the texture island. Terrain, projection, broad fit-sheet evacuation,
> and unrelated measurement-tool expansion are not the current implementation
> priority. An interesting new bottleneck elsewhere does not override this."*

Delivered as commit `49fc32e9` — *"Agent please read - prioritize texture
island"*.

| artefact | SHA-256 |
|---|---|
| `ZHAOZHOU_MASTER_REARCHITECTURE_RECOVERY_2026-09-07.txt` | `B024D62590382925CFD0CFA0AB3BB20A97DDC9A4894173EADB30935F48EE7DB9` |
| `ZHAOZHOU_REARCHITECTURE_RECOVERY_AUDIT_2026-09-07.zip` | `E918D9AFB99CBE8C61AA55CCAB6F20A6F6051E40D5949E5526AB5922C50A20C8` |

This file lives beside the RTL it governs, not in a run folder, because
CLAUDE.md records that *"a run folder is the wrong home for anything durable —
every pass creates a new one, so a file left in the current run is orphaned by
the next."* The handoff makes the same point in its own words: *"Merely
committing a document to a different branch is not delivery."*

---

## Session state at the moment of acknowledgement (its action 1)

```
repo      C:/Programmieren/zencrifice/zhaozhou
branch    zixxtrixx-v8-closeout
HEAD      3c152c6a30a4912cfe6cc118b45db402d62b2caa   (level with upstream)
dirty     0 files
run       runs/CLAUDE-RUNS/RUN-20260906-2035-terrain-writeback-f-sheet-evacuation
agents    none live (the FABLE island architect completed earlier today)
jobs      quartus_fit  zhao_geom_project   ~10 CPU-min, running
          ctest        shell_* regression, 12 of 16 passed
          watcher      tools/maintenance/watch_for_owner_brief.sh
```

## The compute handoff, decided explicitly (its action 4)

The handoff says: *"Do not blindly kill every process or launch a competing
fit. Preserve an in-progress frozen specimen's provenance and choose an
explicit safe handoff of compute resources; cancel queued non-texture jobs that
would delay the texture lane."*

**The running `zhao_geom_project` fit is allowed to finish.** It is not a queued
job, it is ~10 minutes into a ~50-minute run, and it is the measurement that
judges a prediction already committed against `zhao_project_core`. Killing it
would destroy that evidence and free nothing the texture lane can use yet,
because the next texture milestone — the fence phase machine — is **RTL work**
that reaches the toolchain only when it is written and tested. It will finish
well before then. That is the reasoning, recorded so the choice can be
disagreed with.

**No further non-texture fit will be queued.** The three that were next in line
are cancelled from the queue and listed below so nobody re-derives them:

* `zhao_terrain_residency_v2 -TopParameters SEQW=20` — the width-tracking
  discriminator for the missing-17-bits finding.
* `zhao_pair_tess_normals` — a refit for TESS's geomorph cuts, which are in any
  case blocked on a walk restructure.
* `zhao_terrain_normals` — a stale leaf row.

**Nothing is reverted.** Action 3: *"Preserve already completed changes and
evidence. Do not reset the branch, delete reports, or undo working RTL simply
because its priority has changed."* Today's terrain and tooling work stays
exactly as committed.

## Where the texture lane actually stands

The V3.1 control-fabric brief's ten instructions, as delivered so far:

| | instruction | state |
|---|---|---|
| FIRST | entity attribution + four-way endpoint classification | **done**, `V31-M0-OWNER-ATTRIBUTION-20260907.md` |
| SECOND | `zhao_texture_v3rq` occupancy repair, audit, mutation | **done**, `V31-O1-QUEUE-ACCOUNTING-20260907.md` |
| THIRD | registered logical credit, CAPACITY split from DEPTH | **done** |
| FOURTH | registered admission **and an acknowledged fence** | **part one done** (owner credit); part two is the next milestone |
| FIFTH.. | ticket window, local control, ready selectors, retirement | not started |
| — | M6's correctness test, required at M0 | **done**, runs as a `WILL_FAIL` lane |

**The next milestone is FOURTH part two**, which §6 of this handoff names
directly: *"FINISH FOURTH: REGISTERED ADMISSION AND AN ACKNOWLEDGED FENCE"*.
Its hazard is already characterised in
`reports/V31-O1-QUEUE-ACCOUNTING-20260907.md` and in the RTL beside
`adm_ready_o`: `wrap_block_c` becomes true on the edge that moves `tail_q` onto
the wrapping slot, so a naively registered permission still admits one vertex
on a non-quiescent island — the generation-reuse hazard the fence exists to
prevent. Case 19 of the adversarial bench catches it and was **fired on purpose**
before any of this was written.

## Standing rule for this lane

> *"A difficult texture step is work to solve, not permission to switch to
> terrain. A genuine external blocker gets a named dependency and evidence;
> independent texture tests, control ablations, and top-storage work remain the
> permitted alternative work, not an arbitrary new subsystem."*
