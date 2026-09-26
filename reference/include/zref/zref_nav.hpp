// zref_nav.hpp — SW.CPUCOLL's NAVIGATION QUERY: hard passability plus composed
// movement cost for a location or a cell, on the CPU, from canonical terrain
// and the same accepted field commands the fabric was given.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS, AND THE DECISION THAT COMMISSIONED IT
// ---------------------------------------------------------------------------
// `reports/OWNER-DECISION-20260926-I34-NAV.md`, owner, 2026-09-26:
//
//   > Navigation truth and its query service belong to SW.CPUCOLL / the CPU
//   > simulation runtime, as the existing terrain ownership contract already
//   > says. Use the same implementation on desktop and ARM where possible.
//   >
//   > This explicitly supersedes the vacation directive's requirement to create
//   > and publish TERRAIN.COMPOSED_NAV in FPGA-local SDRAM.
//   >
//   > Keep FIELD.WRITE.NAV and its behavior. This is an implementation-
//   > ownership decision, not permission to delete navigation, ignore nav
//   > writes, change their meaning, or cut supported field capacity.
//
// The superseded obligation was a DESTINATION (an SDRAM window). The
// replacement obligation is this file: the query the destination was supposed
// to feed. `spec/terrain_rules.md` §7 already said who owned it — "The sim
// (SW.CPUCOLL) owns the canonical mirror of B/D and the nav grid" — and §4.1
// already said how it must be computed: "Every consumer ... reads the same
// composed lattice values and interpolates them on the same triangulation."
//
// ---------------------------------------------------------------------------
// WHY IT LIVES IN `reference/` AND SHIPS INSIDE `zhao_zref`
// ---------------------------------------------------------------------------
// Because that is the library the CPU simulation runtime actually is, on both
// sides of the project, and the owner asked for ONE implementation:
//
//   * `runtime/desktop` links `zhao_zref` (runtime/desktop/CMakeLists.txt:6),
//     and `zgame::Wizards` — the console's only `zcon::GameTruth` — consumes
//     this service through it.
//   * The GAME links the same library: `Upheaval/CMakeLists.txt:56` does
//     `add_subdirectory("${UPHEAVAL_PIN_DIR}/reference" ...)` against a pinned
//     `git archive` export of this very directory, and `uph_engine` links
//     `zhao_zref` at :108. `Upheaval/src/engine/terrain/terrain_set.hpp`'s
//     `TerrainSet` already calls `zref::terrain::column_query` and
//     `zref::render::compose_lattice` from it. So a nav query placed here is
//     reachable by the game's AI the day the pin is bumped, with no second copy
//     and no new transport.
//   * The ARM runtime (`runtime/mister/`, empty today) will link the same
//     library, which is what "the same implementation on desktop and ARM"
//     means in this tree.
//
// A `runtime/`-only service would have satisfied the sentence and reached
// neither the game nor ARM, because `Upheaval`'s pin exports `reference/` and
// `runtime/include` and nothing else.
//
// ---------------------------------------------------------------------------
// THE SEMANTICS ARE CALLED, NEVER RESTATED
// ---------------------------------------------------------------------------
//   evaluation      `zfield::interpret`, through `zref::render::compose_lattice`
//                   — the ONE §4.1 evaluation, at lattice vertices only. This
//                   service does not walk a patch itself and does not evaluate
//                   a program at a non-lattice point.
//   intake          `zref::terrain::kMaxPatchFields` (16) and the §9.1
//                   command-order accept/reject, offered through `FieldList`.
//   coverage        `zref::terrain::covers` — the closed-interval footprint
//                   test, decided once, in one place.
//   cost reduction  `zref::fieldir::compose_nav`, via
//                   `zref::terrain::nav_vertex`. Signed Q16.16, saturating,
//                   command order, ONE floor at zero.
//   interpolation   `zref::terrain::plane_interp` — the SAME §4.3 two-MAD
//                   single-rounded form `column_query` uses for height.
//   passability     `zref::terrain::column_pick` (class) and
//                   `zref::terrain::collision_normal` (slope, owner ruling R1
//                   of 2026-09-19) — FROM THE SAME PICK as the cost, so the
//                   two can never describe different triangles.
//
// There is no arithmetic in the implementation that is not one of those calls,
// apart from the POLICY below, which is a game rule and is knobs.
//
// ---------------------------------------------------------------------------
// HARD PASSABILITY IS NOT A COST, AND A COST CAN NEVER CHANGE IT
// ---------------------------------------------------------------------------
// `compose_nav`'s own docstring: "A delta may make lawful ground cheaper or
// dearer. It may NOT make VOID, OUT, impossible slope or collision-blocked
// terrain walkable: hard passability is a separate truth and this value never
// speaks to it."
//
// So `Result::passable` is decided by the column CLASS and the collision
// NORMAL, and by nothing else. `Result::cost` is not an input to it, is not
// consulted, and is reported as `kBlockedCost` when the location is blocked so
// that a caller which ignores the flag still cannot route through a hole.
// A negative field delta lowers `cost` and cannot move `passable`; there is a
// committed test that drives exactly that case.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS NOT
// ---------------------------------------------------------------------------
// Not a pathfinder, not tactical AI, not an army controller, not a nav-mesh
// builder, not a nav-grid cache with its own persistence. The owner's standing
// authorization of 2026-09-26 fences the packet at "the smallest production
// navigation-cost/passability service and runtime integration" and says "do not
// enlarge the packet until it becomes impossible to schedule". A route search
// is the CALLER's, and `tests/nav/nav_service_directed.cpp` carries a small one
// as a harness to prove the API answers a router correctly — not as product.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zhao_abi.h"  // ZhCmdTerrainField, ZhTransform2fx

#include "zfield/zfield.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_render.hpp"   // render::TerrainPatch, render::TerrainNavSample
#include "zref/zref_terrain.hpp"  // ComposedLattice, ColumnPick, collision_normal

namespace zref {
namespace nav {

// The generated ABI types this service speaks, named here so the public header
// does not depend on reference/src/zrender/internal.hpp (which is where the
// renderer keeps its own copy of these using-declarations).
using zhao_abi::ZhCmdTerrainField;
using zhao_abi::ZhTransform2fx;

/** Why a location cannot be walked. `kNone` means it can. */
enum class Block : uint8_t {
  kNone = 0,
  kNoTerrain = 1,  // the service has no patch: a refusal, never a passable guess
  kOut = 2,        // outside the lattice envelope — open sky (terrain_rules §3.2)
  kVoid = 3,       // VOID_AUTHORED or VOID_BREACHED column
  kSlope = 4,      // steeper than the policy admits (collision normal, ruling R1)
  kDegenerate = 5  // the picked triangle has no normal; refused rather than guessed
};

/**
 * The cost reported for a location that is NOT passable.
 *
 * INT32_MAX rather than 0, and the direction is the whole point: a caller that
 * reads `cost` and forgets `passable` gets the most expensive tile in the
 * world, not the cheapest. The flattering failure would be a blocked cell that
 * costs nothing, which is a router's shortest path straight through a breach.
 */
inline constexpr int32_t kBlockedCost = INT32_MAX;

/**
 * THE POLICY — every navigation number the game gets to choose, named and
 * editable, per CLAUDE.md's rule that a generated or derived value must never
 * become an unadjustable one.
 *
 * None of this is ratified arithmetic. The ratified part is what `compose_nav`
 * does to these numbers once the field deltas arrive; these are the baseline
 * the deltas act on and the slope rule that decides walkability, and they are
 * GAME RULES. They are defaulted to something sane and are expected to be
 * overridden per creature class by the caller when creature-class navigation
 * arrives — which is why `Service` takes a `Policy` by value rather than
 * hard-coding one.
 */
struct Policy {
  /**
   * The authored cost of one unit of lawful, level ground, Q16.16. This is
   * "the authored baseline" the owner's first acceptance item names: with no
   * live field and level ground, `query()` returns exactly this.
   */
  int32_t flat_cost = 1 << 16;  // 1.0

  /**
   * Surcharge at maximum steepness, Q16.16, scaled by (1 - n.y) and added
   * after the field composition. Ground at the walkable limit costs
   * `flat_cost + slope_cost * (1 - min_slope_cos)`.
   */
  int32_t slope_cost = 4 << 16;  // 4.0 at vertical; ~1.5 at the default limit

  /**
   * The walkability limit: the collision normal's +y lane, Q16.16, below which
   * the ground is IMPASSABLE. 1.0 is level; 0 is a vertical wall.
   * 0.6 is ~53 degrees. `Upheaval`'s creature sheet carries the same idea as
   * `max_slope` in degrees ("steeper ground is refused") and the game is free
   * to convert its own value into this one; the console does not own the game's
   * slope limit and must not appear to.
   */
  int32_t min_slope_cos = (3 << 16) / 5;  // 0.6

  /**
   * Whether a field instance stops contributing once
   * `tick > start_tick + duration_ticks`.
   *
   * TRUE, and it is a CHOSEN law, argued: `compose_lattice` clamps `age` at
   * `duration_ticks` rather than dropping the application, so a field left in
   * the list HOLDS ITS FINAL VALUE FOREVER. That is correct for the renderer,
   * whose host resubmits a live set every frame; it is wrong for a navigation
   * service that owns its own list across ticks, because an expired crater
   * would go on slowing the ground for the rest of the level with nothing in
   * the tree to retire it. This is `zref::terrain::velocity_vertex` law V2's
   * argument in a different lane. `Upheaval`'s own host does the same thing
   * explicitly (`src/engine/demo/foundation_lab.cpp:235-242` retires lanes
   * "whose duration has passed"), so the behaviour agrees across the repos.
   * Set false to get the renderer's hold-at-final behaviour instead.
   */
  bool expire_by_duration = true;
};

/** One answer. Every field is meaningful on every return; nothing is a hole. */
struct Result {
  bool passable = false;
  Block block = Block::kNoTerrain;

  /** Composed movement cost, Q16.16, >= 0. `kBlockedCost` when !passable. */
  int32_t cost = kBlockedCost;

  /** What `cost` would be with NO live field. >= 0. `kBlockedCost` when !passable. */
  int32_t authored_cost = kBlockedCost;

  /** Interpolated live_top at the point, fx16 raw. Only meaningful when kSolid. */
  int32_t ground_y = 0;

  /** The collision normal's +y lane, fx16. 1.0 level, 0 vertical. */
  int32_t slope_cos = 0;

  /** Accepted field lanes whose footprint covered the cell's corners (law N1). */
  uint16_t fields_covering = 0;

  /** Of those, the ones that PRESENTED out-lane 3 and became deltas (law N2). */
  uint16_t fields_applied = 0;

  /** The (terrain, field, tick) generation this answer was computed at. */
  uint64_t generation = 0;
};

/** One accepted TerrainField command, as the navigation service holds it. */
struct FieldInstance {
  uint32_t id = 0;                            // this service's handle, for removal
  const zfield::Decoded* program = nullptr;   // nullptr = resource miss; contributes nothing
  ZhCmdTerrainField cmd{};                    // footprint, start_tick, duration, parameters
};

/**
 * THE SERVICE.
 *
 * ---------------------------------------------------------------------------
 * COMPLEXITY, STATED RATHER THAN HIDDEN
 * ---------------------------------------------------------------------------
 * The owner: "do not hide unbounded full-world evaluation behind an apparently
 * cheap API". So:
 *
 *   `query()` / `query_cell()` AT A SETTLED GENERATION is
 *       O(log W + log H) for the binary-search locate, plus a constant
 *       interpolation and one collision normal. No field is evaluated.
 *
 *   THE FIRST QUERY AFTER THE GENERATION MOVES rebuilds the composed lattice:
 *       ONE `compose_lattice` over the patch, O(V + F*V) program evaluations
 *       where V = w*h vertices (1,089 for the canonical 33x33 patch) and F is
 *       the accepted field count, bounded at `kMaxPatchFields` = 16 by the §9.1
 *       intake. Worst case 17,424 `zfield::interpret` calls; typical case far
 *       fewer, because a field only evaluates inside its own footprint.
 *
 *   WHAT BOUNDS IT: one patch. This service answers within ONE registered
 *       patch and returns `kOut` outside its envelope. There is no world walk,
 *       no all-patches sweep and no implicit streaming. A multi-patch island is
 *       the §4.3 sparse directory lookup in front of this — deliberately NOT in
 *       this packet, and `Upheaval::TerrainSet::patch_of` is the shape it takes.
 *
 * ---------------------------------------------------------------------------
 * THE GENERATION, AND WHY THE TICK IS IN IT
 * ---------------------------------------------------------------------------
 * The owner: "The query must use a coherent terrain/field/tick generation.
 * Field creation, evolution, expiry and removal must affect the answer
 * correctly. Do not cache animated fields forever under a dirty-bit rule that
 * only notices stamps."
 *
 * The cache key is (terrain_epoch, field_epoch, tick-if-any-field-is-listed).
 *
 *   * `terrain_epoch` moves on `set_terrain` and `terrain_changed` — the bake,
 *     the breach, the heal. That is the dirty-bit half, and on its own it is
 *     exactly the rule the owner forbids.
 *   * `field_epoch` moves on add, remove, clear and expiry.
 *   * THE TICK participates whenever the accepted list is non-empty. An earth
 *     program's `age` and `phase` input lanes are functions of the tick, so a
 *     live field's answer changes every tick by construction and a cache that
 *     did not notice would serve a frozen wave. When the list is empty the tick
 *     is excluded, so untouched terrain is cached across ticks for free —
 *     which is the case that matters for cost.
 *
 * This is the rule `Upheaval/src/engine/terrain/terrain_set.cpp:186` already
 * reached independently ("if (!lanes_.empty() && tick != tick_) ++generation_"),
 * and it is recorded here as agreement rather than re-derived.
 *
 * NOT THREAD SAFE. `query()` is const and rebuilds a mutable cache, so two
 * threads querying across a generation change race. Single-threaded fixed-tick
 * simulation is what this console is; `Upheaval::TerrainSet` declares the same
 * limitation for the same reason.
 */
class Service {
 public:
  Service() = default;

  // ---- policy -------------------------------------------------------------
  void set_policy(const Policy& p);
  const Policy& policy() const { return policy_; }

  // ---- canonical terrain (owned by the sim; never mirrored from fabric) ----
  /**
   * Register the patch this service answers over. The patch is BORROWED: the
   * caller owns it and must keep it alive, because SW.CPUCOLL's terrain is the
   * canonical copy and a service that copied it would become a second writer's
   * worth of state to keep in step.
   */
  void set_terrain(const render::TerrainPatch* patch, const ZhTransform2fx& xform);

  /** The bake moved the ground (scar, breach, heal). Invalidate. */
  void terrain_changed();

  bool has_terrain() const { return patch_ != nullptr; }

  // ---- the accepted field commands, in command order ----------------------
  /**
   * Offer one accepted TerrainField command. Returns its id, or 0 if the §9.1
   * intake rejected it — `kMaxPatchFields` accepted, the rest counted in
   * `fields_rejected()` and never composed. Appending is COMMAND ORDER and that
   * order is what `compose_nav` accumulates in.
   */
  uint32_t add_field(const zfield::Decoded* program, const ZhCmdTerrainField& cmd);

  /** Explicit removal (the spell was dispelled, the caster died). */
  bool remove_field(uint32_t id);

  void clear_fields();

  /**
   * Advance to `tick`, retiring every instance whose duration has passed when
   * `Policy::expire_by_duration` is set. Call once per simulation tick before
   * querying; querying without it answers at the last tick given, which is
   * correct and is not a silent stale read because the generation says so.
   */
  void begin_tick(uint32_t tick);

  uint32_t tick() const { return tick_; }
  int active_fields() const { return static_cast<int>(fields_.size()); }
  uint32_t fields_rejected() const { return rejected_; }

  // ---- the query ----------------------------------------------------------
  /** Hard passability + composed movement cost at a world point (fx16). */
  Result query(fx16 wx, fx16 wz) const;

  /**
   * The same, addressed by CELL. The point used is the cell's centre on the
   * placed lattice, via `zref::terrain::lattice_lerp(a, b, 1, 2)` — the shared
   * single-rounding lerp, so a router and a point query agree about what the
   * middle of a cell is.
   */
  Result query_cell(int ci, int cj) const;

  int cells_w() const;
  int cells_h() const;

  // ---- instrumentation (asserted to FIRE, never quoted silent) ------------
  uint64_t generation() const;
  uint64_t rebuilds() const { return rebuilds_; }
  uint64_t queries() const { return queries_; }

  /** Live bytes held by the cache. Reported, not estimated. */
  std::size_t cache_bytes() const;

  /** The composed nav lattice, for tests and for a future bulk consumer. */
  const std::vector<int32_t>& nav_lattice() const;
  const terrain::ComposedLattice& lattice() const;

 private:
  void rebuild_() const;
  Result answer_(fx16 wx, fx16 wz) const;

  Policy policy_{};
  const render::TerrainPatch* patch_ = nullptr;
  ZhTransform2fx xform_{};
  uint64_t terrain_epoch_ = 0;
  uint64_t field_epoch_ = 0;
  uint32_t tick_ = 0;
  uint32_t next_id_ = 1;
  uint32_t rejected_ = 0;
  std::vector<FieldInstance> fields_;

  // the cache, and the generation it describes
  mutable terrain::ComposedLattice lat_{};
  mutable std::vector<int32_t> nav_{};       // composed cost per vertex, Q16.16
  mutable std::vector<uint16_t> covering_{}; // covering lanes per vertex (law N1)
  mutable std::vector<uint16_t> applied_{};  // contributing lanes per vertex (law N2)
  mutable std::vector<render::TerrainNavSample> samples_{};
  mutable uint64_t cached_terrain_ = ~0ull;
  mutable uint64_t cached_fields_ = ~0ull;
  mutable uint64_t cached_tick_ = ~0ull;
  mutable bool valid_ = false;
  mutable uint64_t rebuilds_ = 0;
  mutable uint64_t queries_ = 0;
};

}  // namespace nav
}  // namespace zref
