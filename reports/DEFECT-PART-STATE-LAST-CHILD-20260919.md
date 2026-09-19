# DEFECT — the last child of a generation is lost, and no counter says so

**Found:** 2026-09-19, implementing owner ruling
`reports/RULING-I4-COLLISION-SPAWN-20260919.md`.
**Status: REPAIRED 2026-09-19.** See *The repair* at the foot of this file. The
diagnosis below is kept verbatim because it is correct and because the next
person should be able to read the reasoning without re-deriving it.
**Reproducer:** `tests/prod/run_console_core_smoke.ps1` — it printed the loss on
every run and did not fail on it. It now asserts the conservation law instead,
and prints `children written=6`.

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

## Why it was not repaired in the ruling-I4 packet

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

---

# THE REPAIR — 2026-09-19

Both assertions above now exist. `tb_zhao_console_core_smoke.sv` `$fatal`s on
the conservation law instead of `$display`ing a known defect, and the
block-level gate is `tests/particles/part_state_tick_boundary.cpp`.

## The gate does not guess where the window is

A single hand-placed offer is a guess about a two-cycle window, and a guess
that misses reads as a pass. The new suite **sweeps the child's arrival cycle
across the whole tick** (delays 0..24) and holds `chl_valid_i` the way a real
producer does, so all four regions — inside the survivor pass, on the
append-phase edge, inside `S_DONE`, and after the tick — are visited by
construction. Two of its eight checks are positive controls on the sweep
itself, because a sweep that silently missed the window would satisfy every
other check while testing nothing.

On the **pre-repair** block it fails, with the arrival cycle named:

    delay= 9  t1{accept=1@ 9 after_pass=1 wrote=0 drop=0 stall=0}
    delay=10  t1{accept=1@10 after_pass=1 wrote=0 drop=0 stall=0}
    FAIL: a child ACCEPTED at the handshake is written in the tick that
          accepted it (0 = no record left the machine uncounted):
          expected 0x0, got 0x2
    FAIL: every refused OFFER moved staging_stall_cycles_o -- no refusal is
          silent: expected 0x0, got 0x2
    FAIL: conservation across the sweep: accepted == written +
          dropped_capacity: expected 0x17, got 0x19
    FAIL: the sweep actually REACHED the closed boundary (a child offered to a
          live tick and refused for all of it): expected 0x1, got 0x0
    4/8 checks FAILED

After the repair, 8/8, `accepted=25 written=25 dropped=0 orphans=0`.

## Why the loss is IMPOSSIBLE and not merely rarer

This report warned that closing case (1) alone is the flattering direction.
The repair is not a narrowed window; it is a closed enumeration.

1. **A child can only ENTER staging in `S_SURVIVE` or `S_APPEND`.**
   `chl_ready_o` is now `!chl_full_c && ((st_q == S_SURVIVE) || (st_q ==
   S_APPEND))` — the two phases that can still drain it. `S_DONE` and `S_IDLE`
   refuse at the handshake, so the producer HOLDS and the child belongs to the
   next generation. That is option (b) of this report, declared in
   `PART.STATE.md`.
2. **The append phase cannot close underneath an accept.** Its exit gained
   `&& !chl_wr_fire_c`, which is *the acceptance's own expression* — the same
   wire the write into `chl_m` is gated on, read on the same edge. The original
   exit differenced two quantities clocked differently (`chl_empty_c` from the
   registered pointers, against an accept writing those pointers on that edge),
   which is this tree's detector law applied to a state transition. They are now
   one quantity.
3. **Therefore staging is provably EMPTY on entry to `S_DONE`**, and nothing can
   put anything into it in `S_DONE` or `S_IDLE`. `a_staging_empty_at_done` and
   `a_no_accept_after_append` assert exactly that, and both can fail.
4. **The assignment that actually destroyed the record is gone.** The
   `chl_wp_q`/`chl_rp_q` reset at `tick_start_i` is a no-op given (3), and it is
   REMOVED rather than kept — so if (2) were ever weakened, a late child would be
   written at the head of the next generation instead of vanishing. Deferred, not
   lost. Only `rst_n` clears the pointers, which the contract already declares.

A record in `chl_m` now leaves by exactly three doors and there is no fourth:
the write channel (`children_written_o`), the capacity drop
(`children_dropped_capacity_o`), and `rst_n`. It cannot be overwritten
(`chl_full_c` guards the write pointer and the occupancy is true at every
instant now that nothing zeroes the pointers mid-stream), and it cannot be
abandoned at a phase boundary.

## And the refusal is countable

`staging_stall_cycles_o` counted `chl_valid_i && chl_full_c && (st_q != S_IDLE)`
— a condition **structurally incapable** of seeing a refusal that is not caused
by a full FIFO. Had the repair been shipped with it unchanged, a silent loss
would have been traded for a silent stall, which is the same disease one size
smaller. It now counts every cycle in which a child is **offered and not
accepted**, for any reason. `part_state_tick_boundary.cpp` asserts that no
refused offer is silent, and that check is one of the four that fail on the
pre-repair block.

## Measured

* `run_console_core_smoke.ps1`: `children written=6`,
  `spawn_by_event=[0 0 6 0]`, `requested=6 emitted=6 written=6 dropped_cap=0
  staging_stalls=0`, particles `written=12` (six survivors, six children). Was
  five children and eleven records.
* `part_state_directed` 78, `part_state_capacity_backstop` 9,
  `part_state_child_order_control` 5, `part_spawn_directed` 22 — all unchanged.
  The new work is a separate suite (`part_state_tick_boundary`, 8 checks) for the
  reason `part_state_capacity_backstop.cpp` gives in its own header: those counts
  are quoted as evidence and must not move when somebody adds a case.
* `tests/mutants/zhao_part_state_child_order_mutant.sv` is a COPY of this block
  and was re-cut onto the repaired body, carrying its one mutation forward and
  nothing else (104 inserted lines against production, 0 deleted: its header plus
  the MUTATION block). Its inverted-polarity driver still passes 5/5.
