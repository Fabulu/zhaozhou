# Ruling I4 — collision spawn, and the retirement of `col_*_i`

**Authority:** owner, 2026-09-19 — *"Go with your recommendations on the whole
goal run."* Recorded here so the decision is citable rather than inferred from a
commit.

## The defect being ruled on

Three contracts form a closed contradiction:

| contract | says |
|---|---|
| `PART.COLLIDE.md` | *"the updated particle from PART.UPDATE"*, notes *"leaf"* — **downstream, no return edge** |
| `PART.UPDATE.md` | inputs are record, species, field sample — **no collision velocity** |
| `PART.SPAWN.md` | the four frozen events, **collision among them**, arrive *"from PART.UPDATE"* |

So **PART.UPDATE must announce a collision it cannot observe.** `col_valid_i`
is the seam cut to escape that, and it has no legal producer: driving it from
PART.COLLIDE closes a cycle.

**The measurable consequence, and it is a dead feature, not a cosmetic issue:**
`part_spawn_by_event2_o` is **structurally stuck at zero**. Event bit 2 is
collision. **Spawn-on-collision — sparks on impact, one of the four events
frozen by owner ruling 2026-08-31 §2.4 — does not work in this console**, and
its counter reads zero exactly as it would if nothing had ever collided. It sits
beside `part_collisions_applied_o`, also stuck at zero, which is the two-stuck-
counters pattern CLAUDE.md says to check hardest.

## The ruling

**1. `col_valid_i` / `col_vx_i` / `col_vy_i` / `col_vz_i` are RETIRED from
PART.UPDATE.** They are unreachable by construction and their datapath is
constant-folded by synthesis today, so removing them changes no behaviour and
**reduces** PART.UPDATE's area. `PART.UPDATE.md`'s step 6 is amended to say the
collision response is applied by PART.COLLIDE, not consumed here.

**2. PART.COLLIDE emits the collision event it already owns.** It is the only
block that observes a collision, it already computes the resolved response
(`vout_c`, `pout_c`) and already sets `kPartCollidedThisTick` in the record.
`PART.SPAWN.md`'s "from PART.UPDATE" is amended to "from PART.COLLIDE" — the
event's *content* is unchanged, only its announced origin, which was wrong.

**3. A collision-spawned child is placed at the POST-CONTACT position.**

This is the one genuinely unruled question and it is decided here:

* it is what PART.COLLIDE already computes (`pout_c`), so no new arithmetic is
  invented and no second placement law enters the tree;
* pre-contact places the spark inside the surface the particle just hit, which
  is the visible artefact the whole collision response exists to remove;
* the ratified `kPartStuck` semantics already describe a particle at rest ON the
  surface, so post-contact is the position the rest of the record agrees with.

**Reversal is one constant**, named in the RTL, if it reads wrong on screen.
CLAUDE.md's art law applies: this is a call to be checked by looking at sparks in
motion, not by reasoning about it further.

## What this does NOT license

* **No change to the collision arithmetic itself.** The response PART.COLLIDE
  computes is ratified and untouched.
* **No adapter in `zhao_console_core`.** The core's own header forbids
  "arithmetic invented in the composer", and this ruling is specifically about
  removing a seam, not adding one.
* **No new event.** The four events stay frozen at birth / age marker /
  collision / death per ruling 2026-08-31 §2.4.

## Acceptance

`part_spawn_by_event2_o` must be **seen to move** in a directed case. A counter
that could not fire, and now can, is the whole point — and per CLAUDE.md the
correct behaviour is asserted, with the counter's movement as a separate
positive control, rather than writing a test that asserts the defect.
