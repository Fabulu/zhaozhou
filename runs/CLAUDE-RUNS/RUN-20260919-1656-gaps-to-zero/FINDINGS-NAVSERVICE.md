# FINDINGS — NAVSERVICE

**Branch:** `gz/navservice` · **Base:** `10721cc4` · **Date:** 2026-09-26
**Spec:** `reports/OWNER-DECISION-20260926-I34-NAV.md`
**Register:** **4 before, 4 after.** See §9 — that is correct and is not a
failure to close anything.

---

## 1. THE SERVICE — where it lives, what it is, who can call it

**`zref::nav::Service`**
· header `reference/include/zref/zref_nav.hpp`
· implementation `reference/src/znav/nav_service.cpp`
· ships inside the `zhao_zref` static library.

```cpp
set_policy(Policy) / policy()
set_terrain(const render::TerrainPatch*, ZhTransform2fx) / terrain_changed()
add_field(const zfield::Decoded*, const ZhCmdTerrainField&) -> uint32_t id
remove_field(id) / clear_fields() / begin_tick(uint32_t)
query(fx16 wx, fx16 wz)    -> Result    // a location
query_cell(int ci, int cj) -> Result    // a cell, at its placed centre
Result { bool passable; Block block; int32_t cost, authored_cost,
         ground_y, slope_cos; uint16_t fields_covering, fields_applied;
         uint64_t generation; }
```

### Why `reference/` and not `runtime/`

The brief points at `runtime/mister/.gitkeep`. **A service placed there would
have reached neither the game nor ARM**, and I checked rather than assumed:

* `Upheaval/CMakeLists.txt:56` does
  `add_subdirectory("${UPHEAVAL_PIN_DIR}/reference" ...)` against a pinned
  `git archive` export, and `uph_engine` links `zhao_zref` at `:108`. The pin
  exports `reference/` and `runtime/include`; **`runtime/mister/` is in no
  CMakeLists at all.**
* `Upheaval/src/engine/terrain/terrain_set.cpp` already calls
  `zref::terrain::column_query` and `zref::render::compose_lattice` out of that
  library, so a nav query beside them is reachable by the game's AI the day the
  pin is bumped — no second copy, no new transport.
* `runtime/desktop` links the same library, and the future `runtime/mister`
  will. That is what *"the same implementation on desktop and ARM"* means here.

### Named callers, not a claim of callability

| caller | what it does with it |
|---|---|
| `zgame::Wizards::advance_wizard` (`runtime/include/zgame/wizards.hpp`) | refuses a step into impassable ground (axis-separated, so a wizard slides along a wall); scales the step by the composed cost |
| `runtime/desktop/desktop_main.cpp` | owns the `TerrainPatch` as a host resource, attaches the service, offers a live MIRE field, advances it through `zcon::TickObserver` |
| `Upheaval` `uph_engine` | links the same `zhao_zref`; `TerrainSet` and the p21 creature AI are the intended consumers |

Measured on a real 120-tick desktop run:
`nav: 1 fields live, 240 steps refused, 120 steps cost-scaled, 120 rebuilds /
720 queries, 23944 cache bytes`, and `--replay` reports an identical hash
stream.

**`zcon::TickObserver` is new** (`runtime/include/zcon/zcon.hpp`) and exists for
a defect, not for tidiness: `Session::replay_and_compare` advances the
simulation internally, so a host with tick-dependent resources had no way to
advance them in step and a navigated recording would have diverged at tick 1 —
**reported as a gameplay desync**, which is the worst possible place for that
message.

---

## 2. WHAT I REUSED vs WROTE — and the repositories searched

**Searched:** `zhaozhou` (this worktree — `reference/`, `runtime/`, `tools/`,
`sim/`, `emulator/`, `demos/`, `spec/`, `design/`, `compiler/`, `fpga/rtl/`,
`tests/`, `reports/`), **`Upheaval/`** (`src/`, `tests/`, `tools/`, `docs/`),
**`nanquan/`** (`src/`, `tests/`, `spec/`), and — found only by looking —
**`C:\programmieren\zencrifice\_lanes\`**, five sibling Upheaval worktrees.

### REUSED (called, never re-transcribed)

| thing | where | what it gave |
|---|---|---|
| `zref::fieldir::compose_nav` | `zref_fieldir.hpp:121` | THE reduction: signed Q16.16, saturating in command order, one zero floor |
| `zref::render::compose_lattice` | `zrender/terrain.cpp:227` | the ONE §4.1 evaluation. **It already computed `out[3]` at every covered vertex and threw it away** — the "computed-but-unread lane" the decision names |
| `zref::terrain::column_pick` | `terrain_core.cpp:51` | the §4.3 locate + triangle pick |
| `zref::terrain::collision_normal` | `terrain_core.cpp:78` | the slope, owner ruling R1 of 2026-09-19 |
| `zref::terrain::covers` / `kMaxPatchFields` | `zref_terrain_patch.hpp` | the §9.1 closed-interval test and the 16-field intake bound |
| `zref::terrain::lattice_lerp` | `zref_terrain.hpp:99` | the shared single-rounding lerp, for the cell centre |
| `zfield::decode` / `interpret` | `reference/src/zfield/` | the one interpreter, mirrored in TypeScript and pinned by `.zvec` goldens |
| **nanquan already emits `nav_cost`** | `nanquan/src/field_ir/{crater_ring,impact_wave,wave_pool}.ts` | three committed spells write `nav_cost = height/4`; the images are committed at `compiler/tests/generated/*.zprog`. **The test decodes and runs `crater_ring`** |
| `Upheaval`'s generation rule | `Upheaval/src/engine/terrain/terrain_set.cpp:186` | `if (!lanes_.empty() && tick != tick_) ++generation_` — the same tick-in-the-key rule, reached independently. **Recorded as agreement, not re-derived** |
| `Upheaval`'s lane retirement | `Upheaval/src/engine/demo/foundation_lab.cpp:235-242` | the host drops lanes past `start + duration` — the same expiry rule |

### WROTE

* `reference/include/zref/zref_terrain_nav.hpp` — `nav_vertex`, the per-vertex
  reduction. **A thin view onto `compose_nav`**, built as the exact sibling of
  `zref_terrain_velocity.hpp`, with two laws stated because `compose_nav`'s
  signature does not cover them: **N1** a non-covering lane is SKIPPED not
  zeroed; **N2** an ABSENT out-lane is not a write of zero.
* `reference/include/zref/zref_nav.hpp` + `reference/src/znav/nav_service.cpp`.
* `compose_lattice` gains an optional `nav_out` recorder — the shape
  `velocity_out` already had — carrying vertex index, lane index and
  **presence**. No second walk of the patch.
* `zref::terrain::plane_interp` **exposed**: the §4.3 two-MAD single-rounded
  interpolation, with `column_query` re-implemented on top of it. One
  interpolation law, one home — the consolidation `column_pick` did for the
  locate, one level down.
* `zcon::TickObserver`.
* Two test files and the ledger/contract/RTL classification prose.

**Nothing in the implementation is arithmetic I chose**, except the POLICY
(`zref::nav::Policy`: `flat_cost`, `slope_cost`, `min_slope_cos`,
`expire_by_duration`), which is a game rule and is knobs.

---

## 3. THE FIVE ACCEPTANCE ITEMS

All against the **production API**. `tests/nav/nav_service_directed.cpp`:
**113 checks, 0 failures**. `tests/runtime/wizards_nav_directed.cpp`:
**77 checks, 0 failures**.

**1. No-field results match the authored baseline.** On level ground with no
field, `cost == flat_cost` **exactly** (an identity, not a tolerance: a
constant plane interpolates to itself, and `compose_nav` with zero deltas
returns a non-negative baseline unchanged). Swept over EVERY passable cell of
the fixture: 0 mismatches out of 700+. **`[control]`** the same sweep with a
field live moves 700+ cells, so the equality is not one that could never fail.

**2. A nonzero field changes the queried cost in its covered region.** The
**real shipped `crater_ring`**, decoded from its committed `.zprog`: 224 cells
covered, 38 cells whose cost moved, **0 cells outside the footprint moved**.
Plus a controlled `+1.0` delta checked against `compose_nav` by hand.

**3. Route selection responds, then responds to expiry and removal.** A
4-neighbour Dijkstra whose only truth is `query_cell`. Baseline: 21 steps, cost
21.0, straight along the row. With a `+4.0` band over rows 15–17: 27 steps,
cost 43.0 — **longer in steps and dearer**. At `start_tick + duration + 1` the
instance retires and the route returns to **exactly** 21 steps / 21.0.
`remove_field` does the same by dispel. **`[control]`** removing a retired id
returns `false`.

In the WIRED path: a wizard walks to the lip of a chasm and stops (59
refusals), **and at no tick does the wizard stand on an impassable cell** —
asserted every tick, not once at the end. A `+3.0` mire quarters the distance
covered **exactly** (cost 4.0), the scaling counter moves, **`[control]`** it
does not move on unmodified ground, and after expiry the clean distance
returns.

**4. Hard-blocked terrain stays blocked, negative deltas included.** VOID →
`kVoid`; a 3:1 ridge → `kSlope`; off-envelope → `kOut`; all report
`cost == INT32_MAX`, never 0 — a caller that ignores `passable` gets the most
expensive tile, not the cheapest. Under a **−1000.0** delta over the whole
patch all three stay blocked with unchanged reasons, and **no route crosses the
ridge at any cost**. **`[control]`** the same delta floored lawful ground's cost
at 0 with `fields_applied == 1`, so the two negatives are not holding because
nothing happened.

**5. Overlap, saturation, command order, generation.** Two overlapping fields
ADD (1.0 + 2.0 = 3.0, `fields_applied == 2`). Three `INT32_MAX` deltas
saturate, never wrap. **Command order is observable**: `{+MAX, −MAX/2}` and
`{−MAX/2, +MAX}` give different answers, and **both are checked against
`zref::fieldir::compose_nav` called directly with the same ordered deltas**.
A three-out-lane program covers but contributes nothing (`fields_covering 1`,
`fields_applied 0`); **`[control]`** the four-lane version contributes. An
animated field's cost moves with the tick (`nav_cost == age`, checked at ticks
10 and 40); with no field listed the tick alone causes **zero** extra rebuilds;
`terrain_changed()` invalidates. The §9.1 intake accepts exactly 16 of 20
offered and **counts the other 4**.

---

## 4. CPU WORK AND MEMORY — and the target, labelled

**Measured on the x86-64 DESKTOP build host (winlibs g++ 16.1, `-O1`,
static).** Four live `crater_ring` instances on one 33×33 patch:

| | |
|---|---:|
| rebuild + first query of a tick | **~457 µs / tick** |
| steady-state query, cache warm | **~133 ns / query** |
| cache for one patch | **60,120 bytes** (read from `cache_bytes()`, not estimated) |
| rebuilds / queries over the run | **200 / 200,200** |

**ARM / HPS PERFORMANCE IS UNVERIFIED.** It has not been measured on target and
must not be inferred from the above. The test prints that sentence itself.

**Complexity, stated rather than hidden** (the owner's second prohibition):

* `query` at a settled generation: **O(log W + log H)** for the binary-search
  locate plus a constant interpolation and one collision normal. **No field is
  evaluated.**
* First query after the generation moves: **one** `compose_lattice`,
  **O(V + F·V)** with V = 1,089 vertices and F ≤ 16 by the §9.1 intake — worst
  case 17,424 `interpret` calls, typically far fewer because a field only
  evaluates inside its own footprint.
* **What bounds it: ONE PATCH.** Outside the envelope the answer is `kOut`.
  There is no world walk, no all-patches sweep, no implicit streaming. A
  multi-patch island is the §4.3 sparse directory in front of this, and it is
  deliberately not in this packet.
* **The rebuild counter is the proof**, and it is asserted: exactly one rebuild
  per tick and **none** during the 200,000-query loop. A cheap-looking API with
  a hidden walk would show it there.

---

## 5. NAV-SPECIFIC HARDWARE PATHS — retired vs classified

| path | disposition | why |
|---|---|---|
| `TERRAIN.COMPOSED_NAV` SDRAM region (both the directive's `[0x05AB_0000, 0x05CB_0000)` and DECISION RECORD 1's relocation to `[0x05C4_0000, 0x05E4_0000)`) | **RETIRED** | Struck by the decision. It never existed in RTL or in `zhao_pkg.sv` — grep returns only documents — so the retirement is documentary. Recorded in `zhao_console_core.sv`'s I34 entry beside the stale-address note so it is not chased again. |
| `nav_cost_o` on `zhao_field_earth_adapter` | **CLASSIFIED, kept** | Three sufficient reasons, written into the port's own comment: (1) `FIELD.WRITE.NAV` is explicitly PRESERVED and this is the op's output; (2) it is ordinal 3 of `field-ir.md §7.1`'s frozen earth record `{height, velocity, material, nav_cost}` and removing it is an ABI change the decision's compatibility clause forbids; (3) the differential against `zref::fieldir::compose_nav` via `zhao_field_sinks` is what keeps CPU and fabric agreeing what a tile costs. Cost of keeping: one 32-bit register and a mux leg. **It is not waiting for a consumer — it will not have one.** |
| `efa_nav_cost` wire and its `.nav_cost_o()` connection in `zhao_console_core` / `zhao_prod_top` | **CLASSIFIED, kept** | It is the port connection for the above. An empty connection would trip `PINCONNECTEMPTY` under `-Wall` and buy nothing. |
| `zhao_field_sinks`'s NAV accumulator (`nav_o`) | **KEPT, untouched** | It *is* `FIELD.WRITE.NAV` in fabric, explicitly preserved, and it is the pen-mate the CPU is checked against. |
| `ans_present_o` bit 3 | **KEPT** | Presence is a PRESERVED semantic and the CPU side now implements the same rule (law N2). |

**Nothing was deleted.** Where I was unsure, I classified — as the brief asked.

---

## 6. FALSE CLAIMS FOUND

**In the tree (both now fixed in `design/ops.yml`):**

1. **`result_q: spec/qformats.md §nav-layer` is a PHANTOM SPEC REFERENCE.**
   There is no `§nav-layer` in `spec/qformats.md`; the string `nav` has **zero**
   hits in that file. The format is plain Q16.16, i.e. §2.
2. **`notes: Consumed on the FPGA side and mirrored by SW.CPUCOLL`.** The FPGA
   half is false and `zhao_console_core.sv`'s own I34 audit had already measured
   it — `nav_cost_o`'s only reader is nothing. The ledger went on asserting the
   opposite. Corrected, with the correction recorded rather than the sentence
   quietly replaced.

**In the brief / decision record:**

3. **The decision's acceptance item 1 presumes an "authored baseline" that does
   not exist.** There is **no authored nav layer** in the terrain format: A base,
   B scar, C bottom, D cell state, E material, F sheet, G gameplay grid
   `{heat, wet, corrupt, hazard}`, H tint — and G, the only per-gameplay-cell
   plane, has no nav field. `compose_nav`'s `authored_cost` parameter had no
   producer anywhere in the project. Resolved under the delegation as
   **`zref::nav::Policy`** — a named, editable game rule — and said so in the
   header rather than letting a chosen number look like a found one.
4. **Expiry was not satisfiable by the existing evaluation, and nothing said
   so.** `compose_lattice` clamps `age` at `duration_ticks` and the application
   **holds its final value forever**; it never drops. Acceptance item 3's "when
   the field expires" therefore needed a retirement rule, which is a chosen law
   (`Policy::expire_by_duration`) and is argued in the header. Upheaval's host
   does the same thing explicitly, so the two repos agree.
5. **"SW.CPUCOLL … living at `runtime/mister/.gitkeep`"** is rhetorical and
   misleads about where to build. `runtime/mister/` is a 0-byte placeholder in
   **no** CMakeLists; meanwhile `runtime/` holds a real 1,400-line fixed-tick
   host (`zcon.hpp`, `frame_build.hpp`, `session_io.hpp`, `wizards.hpp`,
   `desktop_main.cpp`) that the brief does not mention. Taken literally it aims
   the packet at a directory the game's pin does not export.
6. **`spec/terrain_rules.md:505-507` "specifies the CPU mirror as
   re-derivation"** — those lines say the sim *owns the canonical mirror* and
   that *"the FPGA bake and the sim bake are the same deterministic function"*.
   Substantively compatible; **"re-derivation" is the brief's gloss, not a
   quote**, and it was being passed along as one.
7. Minor: `zhao_terrain_writeback.sv` is at `fpga/rtl/terrain/`, and the T4
   passage is `:28-32`, not `:27-32`.

**A false ABSENCE that a narrower search would have produced:** the campaign's
in-tree audit says *"THERE IS NO NAVIGATION QUERY ANYWHERE IN THIS TREE, IN
EITHER LANGUAGE"*. That is true of the QUERY and I re-confirmed it across four
trees — but **nav_cost has had a real producer all along**: `nanquan` reserves
`nav_cost` as a keyword, types it as earth out-lane 3, and three committed
spells write it. "Navigation has no producer" would have been false, and the
acceptance test would have been weaker for using a synthetic program.

**And one about the neighbouring repo, for whoever needs it next:**
`C:\programmieren\zencrifice\Upheaval\src` is **far behind**
`C:\programmieren\zencrifice\_lanes\upheaval-p21`, which holds the creature AI,
entity store, order system, rotated-sheet collision, wall/ledge probes and
`steering::step_ok` — a real per-step hard-passability function. Any "does the
game already have X" answer computed against `Upheaval/src` alone reads ABSENT
for things that exist.

---

## 7. WHAT I REFUSED

* **To advance `SW.CPUCOLL`'s ledger maturity.** One of its three jobs is done
  (navigation); the canonical B/D scar mirror and the per-beam occlusion DDA
  are not. Advancing on a finished third is the renamed entry the decision
  forbids. The `maturity_log` records the delivery and says why the state did
  not move.
* **To delete any nav hardware path.** See §5 — three of them are load-bearing
  for a law, an ABI or a differential.
* **To open `TERRAIN.COMPOSED_NAV`, or to feed FPGA results back** as a second
  writer of canonical simulation state.
* **To build a pathfinding framework, a nav grid, a nav mesh, or a multi-patch
  directory.** The scope fence. The Dijkstra in the test is a HARNESS and says
  so in its own comment.
* **To claim the completion register moved.** It reads 4 before and 4 after.
* **To claim ARM performance.**
* **To write the randomized differential of the CPU cost against
  `zhao_field_sinks` in this packet.** It is recorded as an open gap in
  `design/contracts/SW.CPUCOLL.md` rather than left as a silent `(tbd)`.

---

## 8. WHAT I GOT WRONG AND CAUGHT

1. **The speed test measured the wrong thing, and the wrong answer was the
   plausible one.** I asserted "slower ⇒ covers less ground" with a chasm ahead;
   the MIRED wizard finished **further**, because its finer steps fitted closer
   to the lip before one was refused. The two are only the same statement on
   open ground. Split into a speed measurement on clear ground and a refusal
   measurement at the chasm, with the reason written beside the test.
2. **My footprint arithmetic was wrong and the service was right.** I classified
   cells 9–22 as "inside" a `[16 m, 48 m]` footprint at 2 m pitch; the §9.1 test
   is CLOSED, so vertices 8–24 are covered and cells 7–24 are touched. The
   assertion failed, I checked the service first, and the service was correct
   both times. The test now names inside / boundary / outside explicitly.
3. **Off-by-one in an expectation:** I asserted `rebuilds == ticks + 1`. It is
   exactly `ticks`. My arithmetic, not the code's.
4. **The desktop host's first run printed `0 steps cost-scaled`.** The mire was
   live, correct, and in a quadrant the wizards never enter — a wire attached
   and provably idle. Moved the footprint onto their path; both counters are now
   non-zero and the host prints them.
5. **I nearly built the service under `runtime/`** because the brief points
   there. Checking `Upheaval/CMakeLists.txt` first showed the game's pin exports
   `reference/` and `runtime/include` and nothing else, and that `runtime/mister`
   is in no build file — a `runtime/`-only service would have satisfied the
   sentence and reached neither the game nor ARM.

---

## 9. THE REGISTER, honestly

**4 before, 4 after** (`I13`, `I34`, `I55`, plus `zhao_terrain_normalmap`
disconnected). **This is not a shortfall.**

The register's I34 row is **TERRAIN.PATCH's FIELD-HEIGHT LANE (`terr_pt_fld_*`)
and its §9.1 intake** — an FPGA boundary tie-off. **Navigation was never what
that row measured.** The nav obligation and the register row are different
things, and building one does not move the other. Reporting otherwise would be
exactly the "renamed entry" the owner's third standing authorization forbids,
and his own sentence is the rule: *"Moving nav between categories must not make
it disappear."*

**The nav obligation itself** — *"implemented, integrated and tested"* — is met
and is recorded in `reports/DOCKET.md` under the decision's own heading, linked
to the superseded FPGA-publication requirement rather than re-litigating it.

---

## Gates at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` (bare) | 4, RC 1 — unchanged from base |
| `check_console_inventory.py` | OK |
| `check_prod_manifest.py` | OK |
| `tests/nav/nav_service_directed` | **113 checks, 0 failures** (built and RUN) |
| `tests/runtime/wizards_nav_directed` | **77 checks, 0 failures** (built and RUN) |
| `tests/runtime/wizards_directed` | 23 checks — **unchanged**, proving the null default |
| `tests/runtime/zcon_session_directed` | 12 checks — unchanged |
| `zhao-desktop --ticks 120 --record` / `--replay` | identical hash stream |
