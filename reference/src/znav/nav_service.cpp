// nav_service.cpp — SW.CPUCOLL's navigation query. Law citations live in
// zref/zref_nav.hpp; nothing here re-derives a numeric law. Every arithmetic
// step is a call into an already-ratified function:
//
//   compose_lattice   the ONE §4.1 field evaluation (zrender/internal.hpp)
//   nav_vertex        -> zref::fieldir::compose_nav (the 2026-08-24 ruling)
//   column_pick       the §4.3 locate and triangle pick
//   collision_normal  the slope, owner ruling R1 of 2026-09-19
//   plane_interp      the §4.3 two-MAD single-rounded interpolation
//   fx_add / fx_mul   qformats §3
//
// The one thing this file computes itself is the POLICY: the authored baseline
// cost and the slope surcharge. Those are game rules, they are knobs on
// zref::nav::Policy, and they are the only numbers here that were chosen rather
// than inherited.

#include "zref/zref_nav.hpp"

#include <algorithm>

#include "../zrender/internal.hpp"  // white-box: compose_lattice, FieldApp
#include "zref/zref_terrain_nav.hpp"
#include "zref/zref_terrain_patch.hpp"

namespace zref {
namespace nav {
namespace {

/**
 * The slope surcharge: `slope_cost * max(0, 1 - n.y)`, Q16.16, saturating.
 *
 * Applied AFTER the field composition and outside it, deliberately. The field
 * deltas act on the authored baseline by the ratified law; the terrain's own
 * steepness is not a field delta and must not enter the command-order chain,
 * where a saturating add could let a wave's delta and the hill's surcharge
 * interact in an order nobody chose. Both are folded by one final saturating
 * add and the cost floors at zero afterwards, which is `compose_nav`'s own
 * shape one level up.
 */
int32_t slope_surcharge(const Policy& p, int32_t n_y) {
  const int32_t kOne = 1 << 16;
  int32_t steep = kOne - n_y;
  if (steep < 0) steep = 0;   // level or better contributes nothing
  if (steep > kOne) steep = kOne;
  return fx_mul(fx16{p.slope_cost}, fx16{steep}, nullptr).raw;
}

}  // namespace

void Service::set_policy(const Policy& p) {
  policy_ = p;
  // The policy is the authored baseline, so a change to it changes every
  // answer. Bumping TERRAIN rather than FIELDS is deliberate: the baseline is a
  // property of the ground, not of any spell.
  ++terrain_epoch_;
  valid_ = false;
}

void Service::set_terrain(const render::TerrainPatch* patch, const ZhTransform2fx& xform) {
  patch_ = patch;
  xform_ = xform;
  ++terrain_epoch_;
  valid_ = false;
}

void Service::terrain_changed() {
  ++terrain_epoch_;
  valid_ = false;
}

uint32_t Service::add_field(const zfield::Decoded* program, const ZhCmdTerrainField& cmd) {
  // The §9.1 intake bound, enforced here because this list IS the accepted
  // list: append in command order, the first kMaxPatchFields win every run
  // identically, the rest are REJECTED and counted -- never evicted, never
  // silently dropped. A service that clamped instead would make a seventeenth
  // field invisible rather than refused.
  if (static_cast<int>(fields_.size()) >= terrain::kMaxPatchFields) {
    ++rejected_;
    return 0;
  }
  FieldInstance f;
  f.id = next_id_++;
  f.program = program;
  f.cmd = cmd;
  fields_.push_back(f);
  ++field_epoch_;
  valid_ = false;
  return f.id;
}

bool Service::remove_field(uint32_t id) {
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].id != id) continue;
    // Erase preserves the order of everything that stays, which is what keeps
    // the remaining list in COMMAND ORDER after a removal.
    fields_.erase(fields_.begin() + static_cast<std::ptrdiff_t>(i));
    ++field_epoch_;
    valid_ = false;
    return true;
  }
  return false;
}

void Service::clear_fields() {
  if (fields_.empty()) return;
  fields_.clear();
  ++field_epoch_;
  valid_ = false;
}

void Service::begin_tick(uint32_t tick) {
  tick_ = tick;
  if (!policy_.expire_by_duration || fields_.empty()) return;
  const std::size_t before = fields_.size();
  fields_.erase(std::remove_if(fields_.begin(), fields_.end(),
                               [tick](const FieldInstance& f) {
                                 // uint64 on purpose: start_tick + duration is
                                 // two uint32s and a level can outlive both.
                                 const uint64_t end = static_cast<uint64_t>(f.cmd.start_tick) +
                                                      f.cmd.duration_ticks;
                                 return static_cast<uint64_t>(tick) > end;
                               }),
                fields_.end());
  if (fields_.size() != before) {
    ++field_epoch_;
    valid_ = false;
  }
}

uint64_t Service::generation() const {
  // The tick participates only while a field is listed -- see the header. The
  // mix is a plain shift-combine and is an IDENTITY, not a hash: two different
  // generations must never collide, because a collision is a stale answer.
  const uint64_t t = fields_.empty() ? 0ull : static_cast<uint64_t>(tick_);
  return (terrain_epoch_ * 1000003ull + field_epoch_) * 4294967311ull + t;
}

int Service::cells_w() const { return lat_.w > 1 ? lat_.w - 1 : (patch_ ? patch_->width - 1 : 0); }
int Service::cells_h() const { return lat_.h > 1 ? lat_.h - 1 : (patch_ ? patch_->height - 1 : 0); }

const std::vector<int32_t>& Service::nav_lattice() const {
  rebuild_();
  return nav_;
}

const terrain::ComposedLattice& Service::lattice() const {
  rebuild_();
  return lat_;
}

std::size_t Service::cache_bytes() const {
  std::size_t b = 0;
  b += lat_.wx.capacity() * sizeof(int32_t);
  b += lat_.wz.capacity() * sizeof(int32_t);
  b += lat_.top.capacity() * sizeof(int32_t);
  b += lat_.bottom.capacity() * sizeof(int32_t);
  b += lat_.cell_state.capacity() * sizeof(uint8_t);
  b += nav_.capacity() * sizeof(int32_t);
  b += covering_.capacity() * sizeof(uint16_t);
  b += applied_.capacity() * sizeof(uint16_t);
  b += samples_.capacity() * sizeof(render::TerrainNavSample);
  b += fields_.capacity() * sizeof(FieldInstance);
  return b;
}

void Service::rebuild_() const {
  const uint64_t tg = fields_.empty() ? 0ull : static_cast<uint64_t>(tick_);
  if (valid_ && cached_terrain_ == terrain_epoch_ && cached_fields_ == field_epoch_ &&
      cached_tick_ == tg)
    return;

  lat_ = terrain::ComposedLattice{};
  nav_.clear();
  covering_.clear();
  applied_.clear();
  samples_.clear();

  cached_terrain_ = terrain_epoch_;
  cached_fields_ = field_epoch_;
  cached_tick_ = tg;
  valid_ = true;
  ++rebuilds_;

  if (patch_ == nullptr) return;

  // ONE evaluation (§4.1). The nav out-lane is recorded by the same walk that
  // produces live_top; nothing here re-walks the patch, and nothing evaluates a
  // program at a non-lattice point.
  std::vector<render::FieldApp> apps;
  apps.reserve(fields_.size());
  for (const FieldInstance& f : fields_) apps.push_back(render::FieldApp{f.program, f.cmd});

  lat_ = render::compose_lattice(*patch_, xform_, apps, tick_, nullptr, nullptr, &samples_);

  const std::size_t n = static_cast<std::size_t>(lat_.w) * static_cast<std::size_t>(lat_.h);
  if (n == 0) return;

  // Every vertex starts at the authored baseline. compose_nav with zero deltas
  // returns a non-negative baseline unchanged, so an uncovered vertex is the
  // baseline EXACTLY and the owner's first acceptance item is an identity.
  int32_t base = policy_.flat_cost;
  if (base < 0) base = 0;  // a negative authored cost is not a cost
  nav_.assign(n, base);
  covering_.assign(n, 0);
  applied_.assign(n, 0);
  if (samples_.empty()) return;

  // Bucket the recorded samples by vertex, PRESERVING COMMAND ORDER within each
  // vertex. compose_lattice iterates applications OUTER and vertices INNER, so
  // walking `samples_` front to back visits each vertex's lanes in the order
  // they were accepted -- a stable counting sort keeps exactly that, and the
  // order is the thing compose_nav's saturating chain depends on.
  std::vector<uint32_t> off(n + 1, 0);
  for (const render::TerrainNavSample& s : samples_) {
    const std::size_t v = static_cast<std::size_t>(s.vj) * static_cast<std::size_t>(lat_.w) + s.vi;
    if (v < n) ++off[v + 1];
  }
  for (std::size_t i = 0; i < n; ++i) off[i + 1] += off[i];

  std::vector<int32_t> deltas(samples_.size(), 0);
  std::vector<uint8_t> present(samples_.size(), 0);
  std::vector<uint32_t> cursor(off.begin(), off.end() - 1);
  for (const render::TerrainNavSample& s : samples_) {
    const std::size_t v = static_cast<std::size_t>(s.vj) * static_cast<std::size_t>(lat_.w) + s.vi;
    if (v >= n) continue;
    const uint32_t k = cursor[v]++;
    deltas[k] = s.nav;
    present[k] = s.present ? 1u : 0u;
  }

  // The reduction, per vertex, through zref::terrain::nav_vertex -- which is a
  // call into zref::fieldir::compose_nav and nothing else. A sample only exists
  // for a COVERED vertex, so law N1's covering test has already been applied by
  // compose_lattice's own footprint check and `covers` is passed as "all true"
  // for the run rather than re-tested here (one implementation, §9.1).
  // A vertex can be covered by at most the accepted-lane count, which the
  // §9.1 intake bounds at kMaxPatchFields -- so these are stack arrays and the
  // reduction allocates nothing per vertex.
  bool cover_run[terrain::kMaxPatchFields];
  bool present_run[terrain::kMaxPatchFields];
  for (int i = 0; i < terrain::kMaxPatchFields; ++i) cover_run[i] = true;

  for (std::size_t v = 0; v < n; ++v) {
    const uint32_t lo = off[v], hi = off[v + 1];
    if (lo == hi) continue;  // uncovered: already at the baseline
    int m = static_cast<int>(hi - lo);
    if (m > terrain::kMaxPatchFields) m = terrain::kMaxPatchFields;  // unreachable: intake bound
    for (int i = 0; i < m; ++i) present_run[i] = present[lo + i] != 0;
    const terrain::NavOut out =
        terrain::nav_vertex(base, &deltas[lo], cover_run, present_run, m);
    nav_[v] = out.cost_fx;
    covering_[v] = static_cast<uint16_t>(out.lanes_covering);
    applied_[v] = static_cast<uint16_t>(out.lanes_applied);
  }
}

Result Service::answer_(fx16 wx, fx16 wz) const {
  Result r;
  r.generation = generation();
  if (patch_ == nullptr || lat_.w < 2 || lat_.h < 2) {
    r.block = Block::kNoTerrain;
    return r;
  }

  // ONE pick. The class, the normal, the height and the cost all come from it,
  // so they cannot describe different triangles -- the consolidation
  // `column_pick` was exposed for on 2026-09-19, used as intended.
  const terrain::ColumnPick pick = terrain::column_pick(lat_, wx, wz);
  if (pick.cls == terrain::ColumnClass::kOut) {
    r.block = Block::kOut;
    return r;
  }
  if (pick.cls == terrain::ColumnClass::kVoid) {
    r.block = Block::kVoid;
    return r;
  }

  const terrain::CollisionNormal cn = terrain::collision_normal(lat_, pick, /*bottom=*/false);
  r.slope_cos = cn.n.y.raw;
  r.ground_y = terrain::plane_interp(lat_, pick, wx, wz, lat_.top.data());

  if (cn.degenerate) {
    // No normal means no slope verdict. REFUSED rather than assumed passable:
    // the flattering direction here is "it is probably fine", and a degenerate
    // triangle is exactly where a router would like to squeeze through.
    r.block = Block::kDegenerate;
    return r;
  }
  if (cn.n.y.raw < policy_.min_slope_cos) {
    r.block = Block::kSlope;
    return r;
  }

  // ---- composed cost ------------------------------------------------------
  // §4.1: nav "reads the same composed lattice values and interpolates them on
  // the same triangulation (§4.3)". So the COMPOSITION happened at the vertices
  // (command order, saturating, floored) and the INTERPOLATION happens here,
  // through the same call column_query uses for height.
  const int32_t composed = terrain::plane_interp(lat_, pick, wx, wz, nav_.data());
  const int32_t surch = slope_surcharge(policy_, cn.n.y.raw);

  int32_t base = policy_.flat_cost;
  if (base < 0) base = 0;

  const int32_t cost = fx_add(fx16{composed}, fx16{surch}, nullptr).raw;
  const int32_t authored = fx_add(fx16{base}, fx16{surch}, nullptr).raw;

  r.passable = true;
  r.block = Block::kNone;
  r.cost = cost < 0 ? 0 : cost;          // the floor, restated once at the top level
  r.authored_cost = authored < 0 ? 0 : authored;

  // The per-vertex counts, reduced to the cell by MAX over its four corners.
  // A count is not an interpolable quantity -- "two fields spoke here" cannot
  // be 1.4 -- and MAX is the answer a caller asking "did any field act on this
  // cell" wants. It is diagnostic and never enters the cost.
  const std::size_t i00 =
      static_cast<std::size_t>(pick.cj) * static_cast<std::size_t>(lat_.w) + pick.ci;
  const std::size_t idx[4] = {i00, i00 + 1, i00 + static_cast<std::size_t>(lat_.w),
                              i00 + static_cast<std::size_t>(lat_.w) + 1};
  for (std::size_t k = 0; k < 4; ++k) {
    if (idx[k] >= covering_.size()) continue;
    if (covering_[idx[k]] > r.fields_covering) r.fields_covering = covering_[idx[k]];
    if (applied_[idx[k]] > r.fields_applied) r.fields_applied = applied_[idx[k]];
  }
  return r;
}

Result Service::query(fx16 wx, fx16 wz) const {
  rebuild_();
  ++queries_;
  return answer_(wx, wz);
}

Result Service::query_cell(int ci, int cj) const {
  rebuild_();
  ++queries_;
  Result r;
  r.generation = generation();
  if (patch_ == nullptr || lat_.w < 2 || lat_.h < 2) {
    r.block = Block::kNoTerrain;
    return r;
  }
  if (ci < 0 || cj < 0 || ci >= lat_.w - 1 || cj >= lat_.h - 1) {
    r.block = Block::kOut;
    return r;
  }
  // The cell centre on the PLACED lattice, through the shared single-rounding
  // lerp -- so query_cell(ci,cj) and query(centre) are the same point by
  // construction rather than by agreement between two roundings.
  const int32_t cx = terrain::lattice_lerp(lat_.wx[static_cast<std::size_t>(ci)],
                                           lat_.wx[static_cast<std::size_t>(ci) + 1], 1, 2);
  const int32_t cz = terrain::lattice_lerp(lat_.wz[static_cast<std::size_t>(cj)],
                                           lat_.wz[static_cast<std::size_t>(cj) + 1], 1, 2);
  return answer_(fx16{cx}, fx16{cz});
}

}  // namespace nav
}  // namespace zref
