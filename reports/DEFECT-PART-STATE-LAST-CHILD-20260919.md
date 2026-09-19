# DEFECT — the last child of a generation is lost, and no counter says so

**Found:** 2026-09-19, implementing owner ruling
`reports/RULING-I4-COLLISION-SPAWN-20260919.md`.
**Status:** NOT REPAIRED. Recorded so it is read rather than re-derived.
**Reproducer:** `tests/prod/run_console_core_smoke.ps1` — it prints the loss on
every run and does not fail on it.

## The measurement

The smoke bench drives six particles into a heightfield, all six collide, and
the collision event now reaches PART.SPAWN. Its own child accounting, printed
from the console's counter ports:

    requested=6  emitted=6  refused=0  written=5  dropped_cap=0  staging_stalls=0

**Six children were emitted and five were written.** Every counter that exists
to explain a missing child reads ZERO. This is the shape CLAUDE.md names in
*"A detector wired to two operands that move together cannot fire"* and in
*"Counters see what pictures cannot"*: the books balance on both sides of the
handshake while a record disappears between them.

## Where it goes

`zhao_part_spawn.spawn_by_event2_o` counts on `chl_fire_c`, which is
`chl_valid_o && chl_ready_i`. It read **6**, so all six children were
**accepted** — `zhao_part_state`'s `chl_ready_o` was high for each, and that
signal is `!chl_full_c && (st_q != S_IDLE)`. So the sixth child was written into
`chl_m` and `chl_wp_q` advanced. It was then never drained.

The exit from the append phase is

```systemverilog
if (chl_empty_c && !wr_v_q) st_q <= S_DONE;
```

and `chl_empty_c` is computed from the **registered** pointers. Two adjacent
cycles lose a child, and only those two, because outside them `chl_ready_o` is
low and PART.SPAWN would simply hold:

1. the child is accepted on the **same edge** the append phase decides it is
   empty — the decision reads the old pointers, `st_q` leaves, and the next
   `tick_start_i` resets `chl_wp_q`/`chl_rp_q` to zero;
2. the child is accepted **in `S_DONE`**, which also satisfies `st_q != S_IDLE`
   and lasts exactly one cycle before `S_IDLE`.

## Why it had never been seen

It is not new. It was **unreachable**: before the ruling of 2026-09-19 no
particle event could ever fire in this console — the collision bit was
structurally stuck at zero and the bench's stimulus set no born flag, no age
marker and an unbounded lifetime — so PART.SPAWN never emitted a child and the
append phase never had one to lose. *A gate that cannot reach the state is not
evidence about the state.*

The re-plumbing that ruling I4 required (PART.SPAWN moved from beside
PART.COLLIDE to behind it) narrows the timing margin by roughly one beat, but it
is not the cause: PART.SPAWN takes several cycles between accepting a parent and
emitting its first child under either topology, so the last child of a
generation always raced the tick boundary.

## Why it is NOT repaired here

It is outside the ruling, and a partial repair would be worse than none.
Closing case (1) alone — `!(chl_valid_i && chl_ready_o)` in the exit condition —
makes the loss *rarer* without making it impossible, which is the flattering
direction: a defect that fires once a minute gets found, one that fires once an
hour gets argued about.

A whole repair is a **tick-boundary decision for PART.STATE**, not a wire:

* either the append phase must not end until PART.SPAWN is idle, which is a new
  producer→consumer edge (`spawn_busy_o`) of the same shape as the capacity
  backstop that closed gap I8;
* or `chl_ready_o` must fall in `S_DONE` and the child must be **declared** to
  belong to the next generation, which is a visible behaviour change and needs
  a sentence in `PART.STATE.md` saying so.

Whichever is chosen, the loss must become **countable**. `PART.STATE.md` already
says a child is never dropped except at capacity; a third way of losing one, with
no counter, contradicts the contract it is implementing.

## What to assert when it is repaired

`children_emitted_o == children_written_o + children_dropped_capacity_o` across
a tick, in the console bench — the conservation law the present counters imply
and do not enforce. And a directed case whose last child is emitted exactly on
the append-phase boundary, so the repair is shown to hold at the edge it was
written for rather than in the easy middle.
