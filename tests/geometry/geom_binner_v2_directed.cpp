// geom_binner_v2_directed.cpp -- Packet-D V1/V2 structural and metadata proof.
//
// The unchanged binner is the byte/cycle oracle. This driver compares every
// exposed ordinary output on every settle point, then independently scoreboards
// all 1,157 metadata bits by accepted triangle identity through tile fan-out and
// deliberate job stalls. The inverse build requires the committed adjacent-
// address mutant to produce exactly the A/B metadata swap and nothing else.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Vtb_geom_binner_v2_pair.h"
#include "verilated.h"

double sc_time_stamp() { return 0.0; }

namespace {

constexpr int kMetaBits = 1157;
constexpr int kMetaWords = (kMetaBits + 31) / 32;
constexpr uint32_t kMetaTopMask = (uint32_t{1} << (kMetaBits - 32 * (kMetaWords - 1))) - 1u;
constexpr int kGridW = 4;
constexpr int kGridH = 3;
constexpr int kTriCap = 8;
constexpr int kChunks = 6;

using Meta = std::array<uint32_t, kMetaWords>;

Vtb_geom_binner_v2_pair* dut = nullptr;  // heap-resident: avoids MinGW teardown stalls
uint64_t cycles = 0;
uint64_t parity_checks = 0;
uint64_t metadata_checks = 0;
uint64_t hold_checks = 0;
int failures = 0;

void fail(const char* what) {
  if (failures < 40) std::printf("FAIL: %s\n", what);
  ++failures;
}

void require(bool ok, const char* what) {
  if (!ok) fail(what);
}

int32_t sign_extend(uint32_t raw, unsigned width) {
  const uint32_t mask = (uint32_t{1} << width) - 1u;
  raw &= mask;
  const uint32_t sign = uint32_t{1} << (width - 1);
  return (raw & sign) ? static_cast<int32_t>(raw | ~mask) : static_cast<int32_t>(raw);
}

uint32_t m21(int32_t v) { return static_cast<uint32_t>(v) & 0x1fffffu; }
uint32_t m23(int32_t v) { return static_cast<uint32_t>(v) & 0x7fffffu; }
uint32_t m12(int32_t v) { return static_cast<uint32_t>(v) & 0x0fffu; }
uint64_t m48(int64_t v) { return static_cast<uint64_t>(v) & 0x0000ffffffffffffull; }

Meta zero_meta() {
  Meta m{};
  return m;
}

Meta low_meta(uint32_t value) {
  Meta m{};
  m[0] = value ? value : 1u;
  return m;
}

Meta high_meta(uint32_t value) {
  Meta m{};
  m[kMetaWords - 1] = (value & kMetaTopMask) ? (value & kMetaTopMask) : 1u;
  return m;
}

Meta all_meta(uint32_t seed) {
  Meta m{};
  uint32_t x = seed ? seed : 0x91e10da5u;
  for (int i = 0; i < kMetaWords; ++i) {
    x = x * 747796405u + 2891336453u;
    uint32_t w = x ^ (x >> 16) ^ (0x01010101u * static_cast<uint32_t>(i + 1));
    if (w == 0) w = 0x80000001u ^ static_cast<uint32_t>(i);
    m[static_cast<size_t>(i)] = w;
  }
  m[kMetaWords - 1] &= kMetaTopMask;
  if (m[kMetaWords - 1] == 0) m[kMetaWords - 1] = (seed & kMetaTopMask) | 1u;
  return m;
}

bool same_meta(const Meta& a, const Meta& b) {
  for (int i = 0; i < kMetaWords; ++i)
    if (a[static_cast<size_t>(i)] != b[static_cast<size_t>(i)]) return false;
  return true;
}

void drive_meta(const Meta& m) {
  for (int i = 0; i < kMetaWords; ++i)
    dut->tri_meta_i[i] = m[static_cast<size_t>(i)];
}

Meta read_meta() {
  Meta m{};
  for (int i = 0; i < kMetaWords; ++i)
    m[static_cast<size_t>(i)] = dut->v2_job_meta_o[i];
  m[kMetaWords - 1] &= kMetaTopMask;
  return m;
}

#define PARITY(field)                                                                    \
  do {                                                                                   \
    if (dut->old_##field != dut->v2_##field) fail("V1/V2 parity: " #field);             \
  } while (false)

void check_parity() {
  ++parity_checks;
  PARITY(tri_ready_o);
  PARITY(tok_req_o);
  PARITY(job_valid_o);
  PARITY(job_ax_o);
  PARITY(job_ay_o);
  PARITY(job_bx_o);
  PARITY(job_by_o);
  PARITY(job_cx_o);
  PARITY(job_cy_o);
  PARITY(job_first_o);
  PARITY(job_last_o);
  PARITY(job_tile_x_o);
  PARITY(job_tile_y_o);
  PARITY(job_src_id_o);
  PARITY(drain_busy_o);
  PARITY(drain_done_o);
  PARITY(tile_references_o);
  PARITY(max_tile_list_depth_o);
  PARITY(triangles_culled_o);
  PARITY(overflow_o);
  PARITY(arena_full_o);
  PARITY(arena_used_o);
}

#undef PARITY

void settle() {
  dut->eval();
  check_parity();
}

void tick() {
  dut->clk = 0;
  settle();
  dut->clk = 1;
  settle();
  ++cycles;
}

struct Edge {
  int32_t kx = 0;
  int32_t ky = 0;
  int64_t kc = 0;
  bool tl = false;
};

struct Tri {
  int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;  // S12.8 subpixels
  Edge e[3];
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  uint16_t src = 0;
  bool token = true;
  Meta meta{};
};

Tri make_tri(int ax_px, int ay_px, int bx_px, int by_px, int cx_px, int cy_px,
             uint16_t src, const Meta& meta, bool token = true) {
  int64_t ax = static_cast<int64_t>(ax_px) * 256;
  int64_t ay = static_cast<int64_t>(ay_px) * 256;
  int64_t bx = static_cast<int64_t>(bx_px) * 256;
  int64_t by = static_cast<int64_t>(by_px) * 256;
  int64_t cx = static_cast<int64_t>(cx_px) * 256;
  int64_t cy = static_cast<int64_t>(cy_px) * 256;
  int64_t area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  if (area < 0) {
    std::swap(bx, cx);
    std::swap(by, cy);
    area = -area;
  }
  require(area > 0, "fixture triangle has positive area");

  Tri t;
  t.ax = static_cast<int32_t>(ax);
  t.ay = static_cast<int32_t>(ay);
  t.bx = static_cast<int32_t>(bx);
  t.by = static_cast<int32_t>(by);
  t.cx = static_cast<int32_t>(cx);
  t.cy = static_cast<int32_t>(cy);
  const int32_t vx[3] = {t.bx, t.cx, t.ax};
  const int32_t vy[3] = {t.by, t.cy, t.ay};
  const int32_t wx[3] = {t.cx, t.ax, t.bx};
  const int32_t wy[3] = {t.cy, t.ay, t.by};
  for (int i = 0; i < 3; ++i) {
    t.e[i].kx = -(wy[i] - vy[i]);
    t.e[i].ky = wx[i] - vx[i];
    t.e[i].kc = static_cast<int64_t>(vx[i]) * wy[i] -
                static_cast<int64_t>(vy[i]) * wx[i];
    t.e[i].tl = (vy[i] == wy[i]) ? (vx[i] < wx[i]) : (vy[i] < wy[i]);
  }
  const int min_sub_x = std::min({t.ax, t.bx, t.cx});
  const int max_sub_x = std::max({t.ax, t.bx, t.cx});
  const int min_sub_y = std::min({t.ay, t.by, t.cy});
  const int max_sub_y = std::max({t.ay, t.by, t.cy});
  t.min_x = std::max(0, min_sub_x / 256);
  t.max_x = std::min(kGridW * 16 - 1, (max_sub_x - 1) / 256);
  t.min_y = std::max(0, min_sub_y / 256);
  t.max_y = std::min(kGridH * 16 - 1, (max_sub_y - 1) / 256);
  t.src = src;
  t.token = token;
  t.meta = meta;
  return t;
}

void drive_tri(const Tri& t) {
  dut->tri_kx0_i = m23(t.e[0].kx);
  dut->tri_ky0_i = m23(t.e[0].ky);
  dut->tri_kc0_i = m48(t.e[0].kc);
  dut->tri_kx1_i = m23(t.e[1].kx);
  dut->tri_ky1_i = m23(t.e[1].ky);
  dut->tri_kc1_i = m48(t.e[1].kc);
  dut->tri_kx2_i = m23(t.e[2].kx);
  dut->tri_ky2_i = m23(t.e[2].ky);
  dut->tri_kc2_i = m48(t.e[2].kc);
  dut->tri_tl_i = static_cast<uint8_t>((t.e[0].tl ? 1u : 0u) |
                                       (t.e[1].tl ? 2u : 0u) |
                                       (t.e[2].tl ? 4u : 0u));
  dut->tri_ax_i = m21(t.ax);
  dut->tri_ay_i = m21(t.ay);
  dut->tri_bx_i = m21(t.bx);
  dut->tri_by_i = m21(t.by);
  dut->tri_cx_i = m21(t.cx);
  dut->tri_cy_i = m21(t.cy);
  dut->tri_min_x_i = m12(t.min_x);
  dut->tri_max_x_i = m12(t.max_x);
  dut->tri_min_y_i = m12(t.min_y);
  dut->tri_max_y_i = m12(t.max_y);
  dut->tri_src_id_i = t.src;
  dut->tok_grant_i = t.token ? 1 : 0;
  drive_meta(t.meta);
}

struct Job {
  uint32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
  bool first = false, last = false;
  uint32_t tile_x = 0, tile_y = 0;
  uint16_t src = 0;
  Meta meta{};
};

Job old_job() {
  Job j;
  j.ax = dut->old_job_ax_o;
  j.ay = dut->old_job_ay_o;
  j.bx = dut->old_job_bx_o;
  j.by = dut->old_job_by_o;
  j.cx = dut->old_job_cx_o;
  j.cy = dut->old_job_cy_o;
  j.first = dut->old_job_first_o != 0;
  j.last = dut->old_job_last_o != 0;
  j.tile_x = dut->old_job_tile_x_o;
  j.tile_y = dut->old_job_tile_y_o;
  j.src = dut->old_job_src_id_o;
  return j;
}

Job v2_job() {
  Job j;
  j.ax = dut->v2_job_ax_o;
  j.ay = dut->v2_job_ay_o;
  j.bx = dut->v2_job_bx_o;
  j.by = dut->v2_job_by_o;
  j.cx = dut->v2_job_cx_o;
  j.cy = dut->v2_job_cy_o;
  j.first = dut->v2_job_first_o != 0;
  j.last = dut->v2_job_last_o != 0;
  j.tile_x = dut->v2_job_tile_x_o;
  j.tile_y = dut->v2_job_tile_y_o;
  j.src = dut->v2_job_src_id_o;
  j.meta = read_meta();
  return j;
}

bool same_structure(const Job& a, const Job& b) {
  return a.ax == b.ax && a.ay == b.ay && a.bx == b.bx && a.by == b.by &&
         a.cx == b.cx && a.cy == b.cy && a.first == b.first && a.last == b.last &&
         a.tile_x == b.tile_x && a.tile_y == b.tile_y && a.src == b.src;
}

bool same_job(const Job& a, const Job& b, bool metadata) {
  return same_structure(a, b) && (!metadata || same_meta(a.meta, b.meta));
}

struct FrameResult {
  std::vector<Job> jobs;
  uint32_t refs_delta = 0;
  uint32_t culled_delta = 0;
  uint32_t max_depth = 0;
  uint32_t arena_used = 0;
  bool overflow = false;
  int metadata_mismatches = 0;
  int done_pulses = 0;
  int stalled_cycles = 0;
  bool busy_seen = false;
};

uint32_t rng_step(uint32_t& s) {
  s = s * 747796405u + 2891336453u;
  const uint32_t w = ((s >> ((s >> 28) + 4)) ^ s) * 277803737u;
  return (w >> 22) ^ w;
}

void validate_stream(const std::vector<Job>& jobs,
                     const std::unordered_map<uint16_t, size_t>& submit_order) {
  int previous_tile = -1;
  for (const Job& j : jobs) {
    const int px = sign_extend(j.tile_x, 12);
    const int py = sign_extend(j.tile_y, 12);
    require(px >= 0 && py >= 0 && (px & 15) == 0 && (py & 15) == 0,
            "job tile is a nonnegative 16-pixel origin");
    const int tx = px >> 4;
    const int ty = py >> 4;
    require(tx < kGridW && ty < kGridH, "job tile lies in active grid");
    const int tile = ty * kGridW + tx;
    require(tile >= previous_tile, "tile drain order is row-major");
    previous_tile = tile;
  }

  size_t i = 0;
  while (i < jobs.size()) {
    size_t end = i + 1;
    while (end < jobs.size() && jobs[end].tile_x == jobs[i].tile_x &&
           jobs[end].tile_y == jobs[i].tile_y)
      ++end;
    for (size_t k = i; k < end; ++k) {
      require(jobs[k].first == (k == i), "job_first marks exactly the tile-list head");
      require(jobs[k].last == (k + 1 == end), "job_last marks exactly the tile-list tail");
      if (k > i) {
        const auto a = submit_order.find(jobs[k - 1].src);
        const auto b = submit_order.find(jobs[k].src);
        require(a != submit_order.end() && b != submit_order.end() && a->second <= b->second,
                "within-tile drain remains submission FIFO");
      }
    }
    i = end;
  }
}

FrameResult run_frame(const std::vector<Tri>& tris, uint32_t stall_seed,
                      bool permit_metadata_mismatch = false) {
  FrameResult out;
  const uint32_t refs_before = dut->old_tile_references_o;
  const uint32_t culled_before = dut->old_triangles_culled_o;
  std::unordered_map<uint16_t, Meta> expected_meta;
  std::unordered_map<uint16_t, size_t> submit_order;
  std::unordered_set<uint16_t> denied;
  for (size_t i = 0; i < tris.size(); ++i) {
    submit_order.emplace(tris[i].src, i);
    if (tris[i].token)
      expected_meta.emplace(tris[i].src, tris[i].meta);
    else
      denied.emplace(tris[i].src);
  }

  dut->job_ready_i = 0;
  dut->tri_valid_i = 0;
  dut->frame_end_i = 0;
  dut->frame_begin_i = 1;
  tick();
  dut->frame_begin_i = 0;

  bool cleared = false;
  for (int guard = 0; guard < 1000; ++guard) {
    settle();
    if (dut->old_tri_ready_o && dut->v2_tri_ready_o) {
      cleared = true;
      break;
    }
    tick();
  }
  require(cleared, "frame clear reaches triangle-ready finitely");

  for (const Tri& t : tris) {
    drive_tri(t);
    dut->tri_valid_i = 1;
    bool consumed = false;
    for (int guard = 0; guard < 1000; ++guard) {
      settle();
      const bool fire = dut->old_tri_ready_o && dut->v2_tri_ready_o;
      tick();
      if (fire) {
        consumed = true;
        break;
      }
    }
    require(consumed, "triangle offer is consumed finitely");
    dut->tri_valid_i = 0;
    dut->tok_grant_i = 0;
  }

  dut->frame_end_i = 1;
  tick();
  dut->frame_end_i = 0;

  uint32_t rng = stall_seed ? stall_seed : 1u;
  bool old_hold = false;
  bool v2_hold = false;
  Job held_old;
  Job held_v2;
  int forced_stall = 0;
  bool forced_this_job = false;
  bool finished = false;

  for (int guard = 0; guard < 200000; ++guard) {
    dut->clk = 0;
    dut->eval();
    check_parity();

    if (dut->old_drain_busy_o) out.busy_seen = true;
    if (dut->old_drain_done_o) {
      ++out.done_pulses;
      require(!dut->old_job_valid_o && !dut->v2_job_valid_o,
              "drain_done never overlaps a held job");
      dut->job_ready_i = 0;
      tick();  // retire the one-cycle pulse before the next frame
      finished = true;
      break;
    }

    if (dut->old_job_valid_o && !forced_this_job) {
      forced_stall = 3;
      forced_this_job = true;
    }
    if (forced_stall > 0)
      dut->job_ready_i = 0;
    else
      dut->job_ready_i = (rng_step(rng) & 3u) != 0u;
    dut->eval();
    check_parity();

    const Job now_old = old_job();
    const Job now_v2 = v2_job();
    if (old_hold) {
      ++hold_checks;
      require(dut->old_job_valid_o && same_job(now_old, held_old, false),
              "old full job output holds bit-for-bit under stall");
    }
    if (v2_hold) {
      ++hold_checks;
      require(dut->v2_job_valid_o && same_job(now_v2, held_v2, true),
              "V2 full job plus all metadata bits hold under stall");
    }

    const bool fire = dut->old_job_valid_o && dut->job_ready_i;
    if (dut->old_job_valid_o && !dut->job_ready_i) {
      held_old = now_old;
      old_hold = true;
      held_v2 = now_v2;
      v2_hold = true;
      ++out.stalled_cycles;
    } else {
      old_hold = false;
      v2_hold = false;
    }

    if (fire) {
      require(dut->v2_job_valid_o, "V2 job valid on every old accepted job");
      require(same_structure(now_old, now_v2), "accepted old/V2 jobs are structurally exact");
      require(denied.find(now_v2.src) == denied.end(), "token-denied triangle emits no job");
      const auto want = expected_meta.find(now_v2.src);
      require(want != expected_meta.end(), "emitted job belongs to an offered granted triangle");
      if (want != expected_meta.end()) {
        ++metadata_checks;
        if (!same_meta(now_v2.meta, want->second)) {
          ++out.metadata_mismatches;
          if (!permit_metadata_mismatch) fail("all 1157 output metadata bits match scoreboard");
        }
      }
      out.jobs.push_back(now_v2);
      forced_this_job = false;
    }

    tick();
    if (forced_stall > 0) --forced_stall;
  }

  require(finished, "frame drain reaches done finitely");
  out.refs_delta = dut->old_tile_references_o - refs_before;
  out.culled_delta = dut->old_triangles_culled_o - culled_before;
  out.max_depth = dut->old_max_tile_list_depth_o;
  out.arena_used = dut->old_arena_used_o;
  out.overflow = dut->old_overflow_o != 0;
  require(out.done_pulses == 1, "drain_done is exactly one observed pulse");
  require(out.busy_seen, "drain_busy asserted during the frame drain");
  require(out.refs_delta == out.jobs.size(), "tile reference counter delta equals drained jobs");
  require(out.stalled_cycles >= static_cast<int>(out.jobs.size()) * 3,
          "every emitted job experienced an exact hold interval");
  validate_stream(out.jobs, submit_order);
  return out;
}

void test_identity_denial(bool inverse_mode) {
  const Meta ma = all_meta(0xa11ce001u);
  const Meta mx = all_meta(0xdead00ffu);
  const Meta mb = all_meta(0xb22ce002u);
  const Tri a = make_tri(2, 2, 14, 2, 2, 14, 0x0100, ma, true);
  const Tri x = make_tri(2, 2, 14, 2, 2, 14, 0x0101, mx, false);
  const Tri b = make_tri(2, 2, 14, 2, 2, 14, 0x0102, mb, true);
  const FrameResult r = run_frame({a, x, b}, 0xabc001u, inverse_mode);

  require(r.jobs.size() == 2, "A/denied-X/B emits exactly two jobs");
  require(r.culled_delta == 1, "A/denied-X/B counts exactly one denial");
  require(!r.overflow, "token denial is not overflow");
  if (r.jobs.size() == 2) {
    require(r.jobs[0].src == a.src && r.jobs[1].src == b.src,
            "denied X does not shift structural identity");
    require(r.jobs[0].first && !r.jobs[0].last && !r.jobs[1].first && r.jobs[1].last,
            "A/B share one tile with exact first/last markers");
    if (inverse_mode) {
      require(same_meta(r.jobs[0].meta, mb) && same_meta(r.jobs[1].meta, ma),
              "mutant signature is exact adjacent A/B metadata swap");
    } else {
      require(same_meta(r.jobs[0].meta, ma) && same_meta(r.jobs[1].meta, mb),
              "production A/B metadata identity survives denied X");
    }
  }
  require(r.metadata_mismatches == (inverse_mode ? 2 : 0),
          "metadata mismatch count has exact A/B signature");
}

void test_metadata_shapes_and_tiles() {
  const Tri low = make_tri(0, 0, 32, 0, 0, 15, 0x0200, low_meta(0x0000005au));
  const Tri high = make_tri(0, 0, 32, 0, 0, 15, 0x0201, high_meta(0x1bu));
  const Tri all = make_tri(0, 0, 32, 0, 0, 15, 0x0202, all_meta(0x13579bdfu));
  const FrameResult r = run_frame({low, high, all}, 0x220022u);
  require(!r.overflow && r.culled_delta == 0, "metadata shape frame has no drops");
  require(r.jobs.size() == 6, "three triangles fan out across exactly two tiles");
  int low_seen = 0, high_seen = 0, all_seen = 0;
  for (const Job& j : r.jobs) {
    if (j.src == low.src) ++low_seen;
    if (j.src == high.src) ++high_seen;
    if (j.src == all.src) ++all_seen;
  }
  require(low_seen == 2 && high_seen == 2 && all_seen == 2,
          "low/high/all-word records each survive multiple tiles");
}

void test_chunk_boundary() {
  std::vector<Tri> tris;
  for (int i = 0; i < 7; ++i)
    tris.push_back(make_tri(2, 2, 14, 2, 2, 14, static_cast<uint16_t>(0x0300 + i),
                            all_meta(0x30000000u + static_cast<uint32_t>(i))));
  const FrameResult r = run_frame(tris, 0x330033u);
  require(r.jobs.size() == 7, "seven records cross the four-reference chunk boundary");
  require(!r.overflow && r.culled_delta == 0, "chunk-boundary frame remains in capacity");
  require(r.max_depth >= 7, "max tile-list depth observes the second chunk");
}

void test_triangle_capacity_wall() {
  std::vector<Tri> tris;
  for (int i = 0; i < kTriCap + 2; ++i)
    tris.push_back(make_tri(2, 2, 14, 2, 2, 14, static_cast<uint16_t>(0x0400 + i),
                            all_meta(0x40000000u + static_cast<uint32_t>(i))));
  const FrameResult r = run_frame(tris, 0x440044u);
  require(r.overflow, "TRI_CAP refusal latches overflow");
  require(r.culled_delta == 2, "TRI_CAP and following wall cull exactly two triangles");
  require(r.jobs.size() == static_cast<size_t>(kTriCap),
          "TRI_CAP stores and drains exactly the first capacity records");
  for (const Job& j : r.jobs)
    require(j.src < 0x0400 + kTriCap, "TRI_CAP-refused metadata emits no job");
}

void test_chunk_overflow_wall() {
  const Tri broad = make_tri(0, 0, 64, 0, 64, 48, 0x0500, all_meta(0x50000001u));
  const Tri after = make_tri(2, 2, 14, 2, 2, 14, 0x0501, all_meta(0x50000002u));
  const FrameResult r = run_frame({broad, after}, 0x550055u);
  require(r.overflow, "chunk exhaustion latches overflow");
  require(r.arena_used == kChunks, "chunk arena stops exactly at configured capacity");
  require(r.jobs.size() == static_cast<size_t>(kChunks),
          "partial broad triangle drains exactly the granted chunk references");
  require(r.culled_delta == 1, "triangle after chunk wall is culled exactly once");
  for (const Job& j : r.jobs)
    require(j.src == broad.src, "chunk wall emits no later triangle or metadata");
}

void test_randomized_frames() {
  uint32_t rng = 0xd2b10001u;
  uint16_t src = 0x1000;
  uint64_t random_jobs = 0;
  int denied_total = 0;
  for (int frame = 0; frame < 32; ++frame) {
    const int n = 1 + static_cast<int>(rng_step(rng) % 5u);
    std::vector<Tri> tris;
    int denied = 0;
    for (int i = 0; i < n; ++i) {
      const int tx = static_cast<int>(rng_step(rng) % kGridW);
      const int ty = static_cast<int>(rng_step(rng) % kGridH);
      const int x = tx * 16;
      const int y = ty * 16;
      const bool token = (rng_step(rng) & 3u) != 0u;
      if (!token) ++denied;
      tris.push_back(make_tri(x + 1, y + 1, x + 15, y + 2, x + 2, y + 15, src++,
                              all_meta(rng_step(rng)), token));
    }
    const FrameResult r = run_frame(tris, rng_step(rng));
    require(!r.overflow, "bounded randomized frame avoids capacity walls");
    require(r.culled_delta == static_cast<uint32_t>(denied),
            "random token-denial counter is exact");
    random_jobs += r.jobs.size();
    denied_total += denied;
  }
  require(random_jobs > 40, "randomized lane emitted a non-vacuous job population");
  require(denied_total > 0, "randomized lane exercised token denial");
  std::printf("  randomized: 32 frames, %llu jobs, %d denied triangles\n",
              static_cast<unsigned long long>(random_jobs), denied_total);
}

void reset() {
  dut->clk = 0;
  dut->rst_n = 0;
  dut->frame_begin_i = 0;
  dut->frame_end_i = 0;
  dut->grid_w_i = kGridW;
  dut->grid_h_i = kGridH;
  dut->tri_valid_i = 0;
  dut->tok_grant_i = 0;
  dut->job_ready_i = 0;
  drive_meta(zero_meta());
  for (int i = 0; i < 5; ++i) tick();
  dut->rst_n = 1;
  tick();
}

[[noreturn]] void hard_exit(int rc) {
  std::fflush(stdout);
  std::_Exit(rc);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  dut = new Vtb_geom_binner_v2_pair;
  reset();

#ifdef EXPECT_GEOM_BINNER_V2_META_ADDR_SWAP_MUTANT
  const int before = failures;
  test_identity_denial(true);
  const bool exact = failures == before && before == 0 && metadata_checks == 2 &&
                     hold_checks >= 4 && parity_checks > 0;
  if (!exact) {
    std::printf("META-ADDRESS MUTANT DID NOT FIRE EXACTLY: failures=%d metadata_checks=%llu "
                "hold_checks=%llu parity_checks=%llu\n",
                failures, static_cast<unsigned long long>(metadata_checks),
                static_cast<unsigned long long>(hold_checks),
                static_cast<unsigned long long>(parity_checks));
    hard_exit(1);
  }
  std::printf("PASS: metadata-address mutant fired exactly (A/B swapped, 2 mismatches; "
              "denied X absent; structural streams/counters exact)\n");
  hard_exit(0);
#endif

  std::printf("== Packet-D binner V1/V2 and 1157-bit metadata ==\n");
  test_identity_denial(false);
  test_metadata_shapes_and_tiles();
  test_chunk_boundary();
  test_triangle_capacity_wall();
  test_chunk_overflow_wall();
  test_randomized_frames();

  require(metadata_checks > 100, "metadata scoreboard checked a non-vacuous record population");
  require(hold_checks > 100, "full-output hold checker fired repeatedly");
  require(parity_checks > 1000, "cycle-level old/V2 parity checker was non-vacuous");

  std::printf("  %llu full metadata job checks; %llu stalled-hold checks\n",
              static_cast<unsigned long long>(metadata_checks),
              static_cast<unsigned long long>(hold_checks));
  std::printf("  %llu cycle-settle parity checks; %llu clocks\n",
              static_cast<unsigned long long>(parity_checks),
              static_cast<unsigned long long>(cycles));
  std::printf("%s: geom_binner_v2_directed (%d failures)\n",
              failures == 0 ? "PASS" : "FAIL", failures);
  hard_exit(failures == 0 ? 0 : 1);
}
