# What is actually left to build

An audit of `design/blocks.yml` against the RTL and test trees, 2026-08-31.
It exists because the ledger's own summary is misleading in both directions, and
"finish the console" needs a real list rather than a status field.

---

## The headline

`design/blocks.yml` holds **92 blocks** and reports:

| maturity | count |
|---|---|
| SPECIFIED | 37 |
| REFERENCE_COMPLETE | 4 |
| UNIT_VERIFIED | 35 |
| RTL_VERIFIED | 16 |

**37 SPECIFIED overstates the remaining work.** Of those 37:

* **6 are `blocked_on: hardware`** and cannot advance regardless of evidence —
  `SYS.PLL`, `SYS.RESET`, `SYS.CDC`, `MEM.SDRAM`, `SW.TOOLS.REPORT`,
  `SW.TOOLS.BOARDPROBE`. They are waiting for a board, not for work.
* **12 are software or profile entries**, which the standing direction puts
  behind hardware.
* **2 are already built and the ledger is simply stale** (below).
* **10 are explicitly waiting on owner decisions** (below).
* **which leaves 7 blocks of real, unblocked hardware work.**

---

## Built, but still listed SPECIFIED — the ledger is stale

| block | the RTL | the gate |
|---|---|---|
| `TERRAIN.PATCH` | `fpga/rtl/terrain/zhao_terrain_patch.sv` | `terrain_patch_directed`, composed 33x33 dual patch |
| `GEOM.WCACHE` | `zhao_geom_wcache.sv` over `zhao_vertex_arena.sv` | 73-check differential, mutation sweep, inductive formal proof |

Both should be advanced. `GEOM.WCACHE`'s shell landed 2026-08-31; its mechanism
had been finished, proved and mutation-swept for a week under a different file
name, which is exactly why a maturity field is not a substitute for looking.

**The audit method matters here.** Searching the RTL for each block's contract
path found only `TERRAIN.PATCH`, and reported `GEOM.WCACHE` as unbuilt — because
`zhao_vertex_arena` implements it without citing it. Searching for a test named
after the block found both. Neither method alone is reliable; a block can be
finished under any name.

---

## Waiting on owner decisions, not on work

Standing direction: *"Do NOT invent game behaviour for the particle-simulation,
compositor or 2D blocks — those need Fabian's decisions."*

| group | blocks |
|---|---|
| particles | `PART.STATE`, `PART.UPDATE`, `PART.COLLIDE`, `PART.SPAWN`, `PART.LADDER` |
| 2D | `TWOD.PLANE`, `TWOD.SPRITE` |
| compositor / post | `POST.GATHER`, `POST.COMPOSITE`, `POST.ECHO` |

Ten blocks. They are specified as blocks but their BEHAVIOUR is a design
decision, and nothing here should guess it.

---

## The seven that are real, unblocked hardware work

| block | purpose | notes |
|---|---|---|
| `GEOM.MESHFETCH` | meshlet descriptors, frustum reject, LOD per governor | head of the geometry front-end |
| `GEOM.VDECODE` | decode compressed vertex data into the skinning format | needs the compressed vertex FORMAT to be pinned |
| `GEOM.LOOM` | bounded transform graph (orbit/aim/billboard/gait/formations) | the largest of the seven, and closest to game behaviour |
| `GEOM.WARP` | Warp8 deformation programs on instanced vertices | consumes `FIELD.SEQ.WARP` |
| `MEASURE.HISTOGRAM` | error-bucket histogram feeding the governor | small; both neighbours built |
| `FORGE.PRIM` | procedural primitive expansion | feeds `GEOM.SETUP` |
| `INPUT.SNAC` | the SNAC controller interface | small, self-contained |

Plus the five `FIELD.SEQ.*` entries (`EARTH`, `WARP`, `FLOW`, `FORMATION`,
`STAMP`), which are **Field IR programs rather than new datapaths** — the Field
engine they run on is built and heavily optimised. They are listed SPECIFIED
because no program has been authored, and authoring one is closer to content
than to hardware.

### Two of them are not as ready as they look

* **`GEOM.VDECODE`** decodes "compressed vertex data". Nothing in `spec/` pins
  that compression format. Building it means choosing it, and the choice
  determines asset size for every mesh in the game.
* **`GEOM.LOOM`** evaluates orbit, aim, billboard, oscillator, spline, gait and
  formation nodes. That is a list of BEHAVIOURS, and the same rule that holds
  the particle blocks applies to at least the gait and formation halves of it.

`MEASURE.HISTOGRAM`, `INPUT.SNAC`, `GEOM.MESHFETCH` and `FORGE.PRIM` are the
four with no such hole in front of them.

---

## And what is blocking outside the block list

Two things, both decisions:

1. **`wmin`, `wmax`, `scale`** — `reports/OPEN-SPEC-DEPTH-QUANTISATION.md`.
   Blocks GEOM.PROJECT's attribute carry, the last piece of the renderer's
   step 6.
2. **The binner arena capacity** — `reports/BINNER_CAPACITY_FOR_8KM_MAPS.md`.
   An army needs ~150x the triangle capacity and ~25x the references. Every
   capacity question in the renderer now leads here, including what ruling 4's
   TriangleContext would cost.

---

## Honest summary

The console is not 37 blocks from done. It is **7 blocks of unblocked hardware
work, 2 ledger updates, 10 blocks waiting on design decisions, 6 waiting on a
board, and 2 open numeric decisions** — with the renderer's per-pixel path built,
composed and measured end to end.

That is a very different picture from the status field, in both directions: less
work remaining than it says, and more of it blocked on things only the owner can
answer.
