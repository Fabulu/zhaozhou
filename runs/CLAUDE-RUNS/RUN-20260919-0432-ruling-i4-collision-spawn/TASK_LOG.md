# Task Log: RUN-20260919-0432 - implement ruling I4 (collision spawn)

**Created:** 2026-09-19 04:32 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260919-0432-ruling-i4-collision-spawn/

---

## Objective

Implement `reports/RULING-I4-COLLISION-SPAWN-20260919.md`, already authorised by
the owner. Retire PART.UPDATE's `col_*_i`, give the collision event to
PART.COLLIDE, place a collision-spawned child POST-CONTACT behind a named
constant, and make `part_spawn_by_event2_o` SEEN TO MOVE. Implement, do not
decide.

---

## Progress Timeline

### 04:32 - started; baselines taken BEFORE touching anything

- completion register: 85 mandatory gaps (20 tie-offs).
- lint, console core closure from `fit_targets.yml`, no waiver file:
  113 `%Warning` lines + `Exiting due to 114 warning(s)`. The brief's "114
  baseline" is the DIAGNOSTIC total. With `tests/shell/v3_closure_inherited.vlt`
  the same lint is RC=0 and silent.
- POSITIVE CONTROL for the lint gate, on a scratchpad copy of the core with one
  instance pin connection deleted: PINMISSING appeared, total rose to 116. The
  gate fires.

### 05:0x - RTL, contracts, block benches; committed

`f4dbd5ed`. Four ports and a counter out of PART.UPDATE; event vector,
spawn-record port, counter and the `CHILD_AT_POST_CONTACT` knob into
PART.COLLIDE; the one-to-two fork moved from PART.UPDATE's output to
PART.COLLIDE's in the composer; header entry I4 deleted; three contracts
amended with correction banners.

Lint back to exactly 114 diagnostics. `part_update_directed` 264 -> 263,
`part_collide_directed` 180 -> 198, both green, both deltas accounted for
line by line.

### 05:3x - the acceptance, in the connected core

`tests/prod/run_console_core_smoke.ps1`, with six particles driven into a
heightfield at +100 under a STICK response:

    spawn_by_event=[0 0 6 0]   collisions_applied=6   contacts_stick=6
    children written=5, all 5 at the POST-CONTACT position y=102

`part_spawn_by_event2_o` moved 0 -> 6. It could not move at all before.

### 05:4x - a defect found on the way, measured and NOT repaired

Six children emitted, five written, every explaining counter zero. Written up
in `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md`, bannered on
`PART.STATE.md`, printed by the smoke bench on every run. Pre-existing and
previously unreachable; repairing it is a PART.STATE tick-boundary decision,
and a partial repair would make the loss rarer rather than impossible.

---

## Decisions Made

- **The fork moved rather than an adapter being added.** Ruling S3 places the
  child at `pout_c`, which only exists downstream of PART.COLLIDE, so PART.SPAWN
  had to move behind it. That is composition, not arithmetic in the composer.
- **Bit 2 is OVERWRITTEN, not OR-ed.** The OR spelling would have linted
  silently and given the collision bit two authors. The waiver plus a
  simulation assertion is the honest version of the same line.
- **`collision_events_o` is a new counter on PART.COLLIDE** rather than a sum of
  the five `contacts_*`: it is wired to the event gate, so it can disagree with
  them, and disagreement is the bug. It also lets the console keep its
  `part_collisions_applied_o` port, which may not change (prod top is off limits).
- **Two dated reports were CORRECTED, not rewritten** -- their numbers belong to
  the tree they were measured from; the stale "step 6 is folded away, so this
  under-counts" claim now carries a banner saying the opposite.

---

## Next Steps

- A fit is owed: PART.UPDATE should measure SMALLER and PART.COLLIDE slightly
  larger. Nothing here has been through `quartus_map`.
- `CHILD_AT_POST_CONTACT = 0` elaborates an untested generate branch.
- Ruling S3's real acceptance is sparks in motion. Nothing was rendered.
- `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md` is open.