// geom_bin_pipe_v2_directed.cpp -- Packet-D full bin-to-resolve gate.
//
// The executable drives the exact zhao_geom_bin_pipe_v2 composition.  It owns
// only external cache-fill, palette, Sheet and framebuffer-ready models.  Three
// current-rast attribute planes are independently evaluated from the committed
// Packet-D arithmetic at every Packet-C admission, and the real fragment/resolve
// outputs are compared with zref::FragmentPipeline and zref::TileResolve.

#include "Vtb_geom_bin_pipe_v2.h"
#include "verilated.h"

#include "zref/zref_fragment.hpp"
#include "zref/zref_tileresolve.hpp"
#include "zref/zref_tilestore.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
#include <vector>

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

namespace {

uint64_t g_cycle = 0;
uint32_t g_checks = 0;
uint32_t g_tests = 0;

[[noreturn]] void fail(const char* what) {
  std::fprintf(stderr, "packet-d directed FAIL cycle=%llu: %s\n",
               static_cast<unsigned long long>(g_cycle), what);
  std::fflush(stdout);
  std::fflush(stderr);
  std::_Exit(2);  // Windows/Verilator teardown is intentionally bypassed.
}

void require(bool condition, const char* what) {
  if (!condition) fail(what);
  ++g_checks;
}

void begin_test(const char* name) {
  ++g_tests;
  std::printf("packet-d test %u: %s\n", g_tests, name);
}

template <std::size_t N>
void clear_wide(VlWide<N>& value) {
  for (std::size_t i = 0; i < N; ++i) value[i] = 0;
}

template <std::size_t N>
void set_bits(VlWide<N>& value, unsigned lo, unsigned width,
              unsigned __int128 bits) {
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned at = lo + bit;
    const uint32_t mask = uint32_t{1} << (at & 31u);
    if ((bits >> bit) & 1u)
      value[at >> 5] |= mask;
    else
      value[at >> 5] &= ~mask;
  }
}

template <std::size_t N>
uint32_t get_bits32(const VlWide<N>& value, unsigned lo, unsigned width) {
  uint32_t out = 0;
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned at = lo + bit;
    if ((value[at >> 5] >> (at & 31u)) & 1u) out |= uint32_t{1} << bit;
  }
  return out;
}

template <std::size_t N>
std::array<uint32_t, N> copy_wide(const VlWide<N>& value) {
  std::array<uint32_t, N> out{};
  for (std::size_t i = 0; i < N; ++i) out[i] = value[i];
  return out;
}

template <std::size_t N>
bool same_wide(const VlWide<N>& value, const std::array<uint32_t, N>& held) {
  for (std::size_t i = 0; i < N; ++i)
    if (value[i] != held[i]) return false;
  return true;
}

uint64_t mask_u64(int64_t value, unsigned width) {
  if (width == 64) return static_cast<uint64_t>(value);
  return static_cast<uint64_t>(value) & ((uint64_t{1} << width) - 1u);
}

unsigned __int128 mask_i128(__int128 value, unsigned width) {
  const unsigned __int128 mask =
      (width == 128) ? ~static_cast<unsigned __int128>(0)
                     : ((static_cast<unsigned __int128>(1) << width) - 1u);
  return static_cast<unsigned __int128>(value) & mask;
}

__int128 floor_div(__int128 n, __int128 d) {
  require(d > 0, "oracle floor_div denominator was not positive");
  if (n >= 0) return n / d;
  return -((-n + d - 1) / d);
}

int32_t div_rhu_s128_oracle(__int128 n, __int128 d) {
  __int128 q = floor_div(n + floor_div(d, 2), d);
  if (q > std::numeric_limits<int32_t>::max())
    return std::numeric_limits<int32_t>::max();
  if (q < std::numeric_limits<int32_t>::min())
    return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(q);
}

struct Plane {
  __int128 n0 = 0;
  __int128 dndx = 0;
  __int128 dndy = 0;
};

Plane affine_plane(int32_t base, int32_t grad_x, int32_t grad_y,
                   int64_t area) {
  Plane p;
  p.n0 = static_cast<__int128>(base) * area;
  p.dndx = static_cast<__int128>(grad_x) * area;
  p.dndy = static_cast<__int128>(grad_y) * area;
  return p;
}

template <std::size_t N>
void drive_plane(VlWide<N>& port, const Plane& p) {
  clear_wide(port);
  set_bits(port, 0, 72, mask_i128(p.dndy, 72));
  set_bits(port, 72, 72, mask_i128(p.dndx, 72));
  set_bits(port, 144, 96, mask_i128(p.n0, 96));
}

int32_t current_rast_attr(const Plane& p, int64_t area, int32_t min_x,
                          int32_t tile_x, int32_t tile_y,
                          uint8_t row, uint8_t col) {
  const int32_t grad = div_rhu_s128_oracle(p.dndx, area);
  const __int128 row_n = p.n0 + p.dndx * min_x +
                         p.dndy * (tile_y + row) +
                         floor_div(p.dndx, 2) + floor_div(p.dndy, 2);
  const int32_t row_q = div_rhu_s128_oracle(row_n, area);
  const uint32_t stepped = static_cast<uint32_t>(row_q) +
      static_cast<uint32_t>(grad) * static_cast<uint32_t>(tile_x - min_x + col);
  return static_cast<int32_t>(stepped);
}

struct Triangle {
  int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
  int32_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  int32_t kx[3] = {}, ky[3] = {};
  int64_t kc[3] = {};
  bool tl[3] = {};
  int64_t area = 0;
};

bool top_left(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  return (ay == by) ? (ax < bx) : (ay < by);
}

Triangle make_triangle(int32_t ax_px, int32_t ay_px,
                       int32_t bx_px, int32_t by_px,
                       int32_t cx_px, int32_t cy_px,
                       int32_t min_x, int32_t max_x,
                       int32_t min_y, int32_t max_y) {
  Triangle t;
  t.ax = ax_px * 256; t.ay = ay_px * 256;
  t.bx = bx_px * 256; t.by = by_px * 256;
  t.cx = cx_px * 256; t.cy = cy_px * 256;
  t.min_x = min_x; t.max_x = max_x;
  t.min_y = min_y; t.max_y = max_y;
  t.area = static_cast<int64_t>(t.bx - t.ax) * (t.cy - t.ay) -
           static_cast<int64_t>(t.by - t.ay) * (t.cx - t.ax);
  require(t.area > 0, "directed triangle did not have positive winding");
  const int32_t vx[3] = {t.bx, t.cx, t.ax};
  const int32_t vy[3] = {t.by, t.cy, t.ay};
  const int32_t wx[3] = {t.cx, t.ax, t.bx};
  const int32_t wy[3] = {t.cy, t.ay, t.by};
  for (unsigned i = 0; i < 3; ++i) {
    t.kx[i] = -(wy[i] - vy[i]);
    t.ky[i] = wx[i] - vx[i];
    t.kc[i] = static_cast<int64_t>(vx[i]) * wy[i] -
              static_cast<int64_t>(vy[i]) * wx[i];
    t.tl[i] = top_left(vx[i], vy[i], wx[i], wy[i]);
  }
  return t;
}

bool covered(const Triangle& t, int32_t x, int32_t y) {
  const int64_t px = static_cast<int64_t>(x) * 256 + 128;
  const int64_t py = static_cast<int64_t>(y) * 256 + 128;
  for (unsigned i = 0; i < 3; ++i) {
    const int64_t e = static_cast<int64_t>(t.kx[i]) * px +
                      static_cast<int64_t>(t.ky[i]) * py + t.kc[i];
    if (e < 0 || (e == 0 && !t.tl[i])) return false;
  }
  return true;
}

struct Material {
  uint8_t sample_count = 0;
  uint8_t binding = 0;
  uint8_t palette_generation = 0;
  uint8_t palette_slot = 0;
  uint8_t response_class = 0;
  uint32_t base_rgb = 0;
  uint8_t base_alpha = 0xff;
  uint8_t recipe = 0;
  uint8_t recipe_weight = 0x5a;
  uint8_t lod = 0;
  bool aux_required = false;
  bool aux_context_nonzero = false;
  uint32_t vertex_rgb = 0xffffff;
  uint8_t vertex_alpha = 0xff;
  uint8_t effect_tag = 0;
  uint8_t stencil = 0;
  uint32_t state = 0;
  uint32_t result_rgb = 0;
  uint8_t result_alpha = 0xff;
  uint8_t result_index = 0;
  uint8_t result_status = 0;
};

struct Job {
  Triangle tri;
  uint16_t source = 0;
  Plane invw;
  Plane u;
  Plane v;
  Material mat;
};

struct ExpectedCandidate {
  uint8_t addr = 0;
  uint32_t depth = 0;
  int32_t u = 0;
  int32_t v = 0;
  uint32_t state = 0;
  uint16_t source = 0;
  Material mat;
};

struct TileOracle {
  int32_t x = 0;
  int32_t y = 0;
  uint16_t source = 0;
  uint16_t index = 0;
  uint16_t coverage = 0;
  bool degenerate = false;
  zref::TileResolve::Out resolved{};
};

uint64_t make_clear_word(uint32_t rgb, uint8_t tag, uint32_t depth,
                         uint8_t stencil) {
  zref::TileStore::Word w;
  w.r = static_cast<uint8_t>(rgb >> 16);
  w.g = static_cast<uint8_t>(rgb >> 8);
  w.b = static_cast<uint8_t>(rgb);
  w.tag = tag;
  w.depth = depth;
  w.stencil = stencil;
  return w.pack();
}

class Harness {
 public:
  VerilatedContext* context = new VerilatedContext;
  Vtb_geom_bin_pipe_v2* dut = new Vtb_geom_bin_pipe_v2{context};

  std::deque<ExpectedCandidate> expected_candidates;
  std::deque<ExpectedCandidate> expected_fragments;
  std::deque<TileOracle> expected_tiles;

  bool validate_candidates = true;
  bool validate_fragments = true;
  bool validate_framebuffer = true;
  bool framebuffer_backpressure = false;
  bool fill_active = false;
  uint32_t fill_line = 0;
  unsigned fill_beat = 0;
  unsigned fill_delay = 0;
  unsigned next_fill_delay = 0;

  uint32_t cand_fires = 0;
  uint32_t fragment_fires = 0;
  uint32_t drop_fires = 0;
  uint32_t tile_done_events = 0;
  uint32_t fb_fires = 0;
  uint32_t drain_done_events = 0;
  uint32_t old_drain_done_events = 0;
  bool saw_green_raw_five = false;
  bool saw_sheet_request = false;
  bool saw_early_swap = false;
  bool saw_second_job_before_first_done = false;
  bool saw_simultaneous_attr_earlyz_drop = false;

  bool capture_outputs = false;
  std::array<uint16_t, 256> captured_v2_rgb{};
  std::array<uint8_t, 256> captured_v2_tag{};
  std::array<uint8_t, 256> captured_v2_addr{};
  std::array<int16_t, 256> captured_v2_x{}, captured_v2_y{};
  std::array<uint16_t, 256> captured_v2_source{};
  std::array<uint16_t, 256> captured_old_rgb{};
  std::array<uint8_t, 256> captured_old_tag{};
  std::array<uint8_t, 256> captured_old_addr{};
  std::array<int16_t, 256> captured_old_x{}, captured_old_y{};
  std::array<uint16_t, 256> captured_old_source{};
  unsigned captured_v2_count = 0;
  unsigned captured_old_count = 0;
  bool captured_v2_done = false, captured_old_done = false;
  uint32_t captured_v2_crc = 0, captured_old_crc = 0;
  uint16_t captured_v2_crc_index = 0, captured_old_crc_index = 0;
  uint16_t captured_v2_cov = 0, captured_old_cov = 0;
  bool captured_v2_degen = false, captured_old_degen = false;

  bool held_cand = false;
  std::array<uint32_t, 16> held_cand_data{};
  bool held_fb = false;
  uint16_t held_fb_rgb = 0;
  uint8_t held_fb_tag = 0;
  uint8_t held_fb_addr = 0;
  int16_t held_fb_x = 0, held_fb_y = 0;
  uint16_t held_fb_source = 0;
  bool held_fb_last = false;
  bool held_old_fb = false;
  uint16_t held_old_fb_rgb = 0;
  uint8_t held_old_fb_tag = 0, held_old_fb_addr = 0;
  int16_t held_old_fb_x = 0, held_old_fb_y = 0;
  uint16_t held_old_fb_source = 0;
  bool held_old_fb_last = false;
  unsigned tile_fb_next = 0;

  Harness() { park(); }

  void park() {
    dut->clk = 0;
    dut->rst_n = 0;
    dut->frame_begin_i = 0;
    dut->frame_end_i = 0;
    dut->grid_w_i = 1;
    dut->grid_h_i = 1;
    dut->frame_clear_word_i = 0;
    dut->tri_valid_i = 0;
    dut->old_enable_i = 0;
    dut->old_job_fill_word_i = 0;
    dut->old_job_state_i = 0;
    dut->old_job_src_a_i = 0xff;
    dut->old_job_texel_rgb_i = 0xffffff;
    dut->old_job_texel_a_i = 0xff;
    dut->old_job_texel_idx_i = 0;
    dut->tok_grant_i = 1;
    dut->frame_fault_clear_valid_i = 0;
    dut->cfg_valid_i = 0;
    dut->cfg_rsp_ready_i = 1;
    dut->fill_req_ready_i = 1;
    dut->fill_data_valid_i = 0;
    dut->fill_data_i = 0;
    dut->fill_refused_i = 0;
    dut->pal_load_valid_i = 0;
    dut->sheet_req_ready_i = 1;
    dut->pg_valid_i = 0;
    dut->pg_op_i = 1;
    dut->pg_status_i = 0;
    dut->pg_tag_i = 0;
    dut->pg_strength_i = 0;
    dut->pg_src_id_i = 0;
    dut->fb_ready_i = 1;
    dut->test_start_enable_i = 31;
    dut->test_attr_cov_enable_i = 7;
    dut->test_stage_admit_enable_i = 1;
    clear_wide(dut->tri_invw_plane_i);
    clear_wide(dut->tri_u_over_w_plane_i);
    clear_wide(dut->tri_v_over_w_plane_i);
    clear_wide(dut->tri_flat_request_i);
    clear_wide(dut->cfg_row_i);
  }

  uint16_t fill_word(uint32_t line, unsigned beat) const {
    if (beat != 0) return 0;
    if (line == 0x00002000u) return 0x0005u;  // CLUT8 raw index 5
    return 0x00c7u;
  }

  void check_candidate() {
    require(!expected_candidates.empty(),
            "Packet-C admitted a candidate absent from the zref plane scoreboard");
    const ExpectedCandidate e = expected_candidates.front();
    expected_candidates.pop_front();
    require(get_bits32(dut->stage_candidate_data_o, 482, 8) == e.addr,
            "candidate in-tile address differed from EDGEWALK order");
    require(get_bits32(dut->stage_candidate_data_o, 458, 24) == e.depth,
            "candidate invw/depth differed from current-rast plane");
    require(static_cast<int32_t>(get_bits32(dut->stage_candidate_data_o, 330, 32)) == e.u,
            "candidate U/W differed from current-rast plane");
    require(static_cast<int32_t>(get_bits32(dut->stage_candidate_data_o, 298, 32)) == e.v,
            "candidate V/W differed from current-rast plane");
    require(get_bits32(dut->stage_candidate_data_o, 426, 32) == e.state,
            "candidate fragment state changed across binner/Early-Z");
    require(get_bits32(dut->stage_candidate_data_o, 410, 16) == e.source,
            "candidate source identity changed across binner/Early-Z");
    require(get_bits32(dut->stage_candidate_data_o, 386, 24) == e.mat.vertex_rgb &&
                get_bits32(dut->stage_candidate_data_o, 378, 8) == e.mat.vertex_alpha &&
                get_bits32(dut->stage_candidate_data_o, 370, 8) == e.mat.effect_tag &&
                get_bits32(dut->stage_candidate_data_o, 362, 8) == e.mat.stencil,
            "candidate continuation tail changed across binner/Early-Z");
    require(get_bits32(dut->stage_candidate_data_o, 296, 2) == e.mat.sample_count &&
                get_bits32(dut->stage_candidate_data_o, 288, 8) == e.mat.binding &&
                get_bits32(dut->stage_candidate_data_o, 277, 3) == e.mat.recipe &&
                get_bits32(dut->stage_candidate_data_o, 20, 24) == e.mat.base_rgb,
            "candidate flat request fields changed across binner/Early-Z");
    expected_fragments.push_back(e);
  }

  void check_fragment() {
    require(!expected_fragments.empty(),
            "Packet-C exposed a fragment absent from admitted-candidate scoreboard");
    const ExpectedCandidate e = expected_fragments.front();
    expected_fragments.pop_front();
    require(dut->stage_fragment_addr_o == e.addr &&
                dut->stage_fragment_depth_o == e.depth &&
                dut->stage_fragment_state_o == e.state &&
                dut->stage_fragment_src_id_o == e.source,
            "fragment boundary lost address/depth/state/source identity");
    require(dut->stage_fragment_texel_rgb_o == e.mat.result_rgb &&
                dut->stage_fragment_texel_a_o == e.mat.result_alpha &&
                dut->stage_fragment_texel_idx_o == e.mat.result_index &&
                dut->stage_fragment_status_o == e.mat.result_status,
            "fragment boundary texture result/raw index differed");
    if (e.mat.result_index == 5 && dut->stage_fragment_texel_rgb_o == 0x00ff00u)
      saw_green_raw_five = true;
  }

  void check_framebuffer_fire() {
    require(!expected_tiles.empty(), "framebuffer beat had no expected tile");
    const TileOracle& tile = expected_tiles.front();
    require(tile_fb_next < 256, "framebuffer emitted more than 256 beats for a tile");
    require(dut->fb_addr_o == tile_fb_next,
            "framebuffer address was not exact raster order");
    require(dut->fb_rgb565_o == tile.resolved.rgb565[tile_fb_next] &&
                dut->fb_tag_o == tile.resolved.tag[tile_fb_next],
            "framebuffer RGB565/tag differed from zref::TileResolve");
    const int row = static_cast<int>(tile_fb_next >> 4);
    const int col = static_cast<int>(tile_fb_next & 15u);
    require(static_cast<int16_t>(dut->fb_x_o) == tile.x + col &&
                static_cast<int16_t>(dut->fb_y_o) == tile.y + row &&
                dut->fb_src_id_o == tile.source,
            "framebuffer coordinate/source did not describe resolving tile");
    require(static_cast<bool>(dut->fb_last_o) == (tile_fb_next == 255),
            "framebuffer last marker was not beat 255");
    ++tile_fb_next;
  }

  void check_tile_done() {
    require(!expected_tiles.empty(), "tile_done had no expected tile");
    const TileOracle tile = expected_tiles.front();
    require(tile_fb_next == 256, "tile_done preceded all 256 framebuffer beats");
    require(dut->tile_crc_o == tile.resolved.crc32c &&
                dut->tile_crc_index_o == tile.index,
            "tile CRC/index differed from zref::TileResolve");
    require(dut->tile_cov_count_o == tile.coverage &&
                static_cast<bool>(dut->tile_degenerate_o) == tile.degenerate,
            "resolve coverage/degenerate shadow differed from finished tile");
    expected_tiles.pop_front();
    tile_fb_next = 0;
    ++tile_done_events;
  }

  struct Events {
    bool cand = false;
    bool fragment = false;
    bool drop = false;
    bool mismatch = false;
    bool fill_req = false;
    bool fb = false;
    bool tile_done = false;
    bool drain_done = false;
    bool old_fb = false;
    bool old_tile_done = false;
    bool old_drain_done = false;
  };

  Events step() {
    dut->fill_data_valid_i = fill_active && (fill_delay == 0);
    dut->fill_data_i = dut->fill_data_valid_i ? fill_word(fill_line, fill_beat) : 0;
    dut->fb_ready_i = framebuffer_backpressure ? ((g_cycle % 5u) != 1u) : 1;

    dut->clk = 0;
    dut->eval();

    if (held_cand)
      require(dut->stage_candidate_valid_o &&
                  same_wide(dut->stage_candidate_data_o, held_cand_data),
              "490-bit stage candidate changed under backpressure");
    if (held_fb)
      require(dut->fb_valid_o && dut->fb_rgb565_o == held_fb_rgb &&
                  dut->fb_tag_o == held_fb_tag && dut->fb_addr_o == held_fb_addr &&
                  static_cast<int16_t>(dut->fb_x_o) == held_fb_x &&
                  static_cast<int16_t>(dut->fb_y_o) == held_fb_y &&
                  dut->fb_src_id_o == held_fb_source &&
                  static_cast<bool>(dut->fb_last_o) == held_fb_last,
              "framebuffer beat changed while ready was low");

    if (held_old_fb)
      require(dut->old_fb_valid_o && dut->old_fb_rgb565_o == held_old_fb_rgb &&
                  dut->old_fb_tag_o == held_old_fb_tag &&
                  dut->old_fb_addr_o == held_old_fb_addr &&
                  static_cast<int16_t>(dut->old_fb_x_o) == held_old_fb_x &&
                  static_cast<int16_t>(dut->old_fb_y_o) == held_old_fb_y &&
                  dut->old_fb_src_id_o == held_old_fb_source &&
                  static_cast<bool>(dut->old_fb_last_o) == held_old_fb_last,
              "old flat framebuffer beat changed while ready was low");

    Events e;
    e.cand = dut->packet_c_cand_fire_o;
    e.fragment = dut->packet_c_fragment_fire_o;
    e.drop = dut->packet_c_drop_fire_o;
    e.mismatch = dut->sequence_mismatch_o;
    e.fill_req = dut->fill_req_valid_o && dut->fill_req_ready_i;
    e.fb = dut->fb_valid_o && dut->fb_ready_i;
    e.tile_done = dut->tile_done_o;
    e.drain_done = dut->drain_done_o;
    e.old_fb = dut->old_fb_valid_o && dut->fb_ready_i;
    e.old_tile_done = dut->old_tile_done_o;
    e.old_drain_done = dut->old_drain_done_o;

    if (dut->sheet_req_valid_o) saw_sheet_request = true;
    if (e.cand) {
      ++cand_fires;
      if (validate_candidates) check_candidate();
    }
    if (e.fragment) {
      ++fragment_fires;
      if (validate_fragments) check_fragment();
    }
    if (e.drop) ++drop_fires;
    if (e.fb) {
      ++fb_fires;
      if (validate_framebuffer) check_framebuffer_fire();
    }
    if (e.tile_done) {
      if (validate_framebuffer) check_tile_done();
      else ++tile_done_events;
    }
    if (e.drain_done) ++drain_done_events;
    if (e.old_drain_done) ++old_drain_done_events;
    if (dut->local_fault_pulse_o && dut->earlyz_hold_valid_o)
      saw_simultaneous_attr_earlyz_drop = true;

    if (capture_outputs && e.fb) {
      require(captured_v2_count < 256, "captured V2 tile exceeded 256 beats");
      const unsigned at = captured_v2_count++;
      captured_v2_rgb[at] = dut->fb_rgb565_o;
      captured_v2_tag[at] = dut->fb_tag_o;
      captured_v2_addr[at] = dut->fb_addr_o;
      captured_v2_x[at] = static_cast<int16_t>(dut->fb_x_o);
      captured_v2_y[at] = static_cast<int16_t>(dut->fb_y_o);
      captured_v2_source[at] = dut->fb_src_id_o;
    }
    if (capture_outputs && e.old_fb) {
      require(captured_old_count < 256, "captured old tile exceeded 256 beats");
      const unsigned at = captured_old_count++;
      captured_old_rgb[at] = dut->old_fb_rgb565_o;
      captured_old_tag[at] = dut->old_fb_tag_o;
      captured_old_addr[at] = dut->old_fb_addr_o;
      captured_old_x[at] = static_cast<int16_t>(dut->old_fb_x_o);
      captured_old_y[at] = static_cast<int16_t>(dut->old_fb_y_o);
      captured_old_source[at] = dut->old_fb_src_id_o;
    }
    if (capture_outputs && e.tile_done) {
      captured_v2_done = true;
      captured_v2_crc = dut->tile_crc_o;
      captured_v2_crc_index = dut->tile_crc_index_o;
      captured_v2_cov = dut->tile_cov_count_o;
      captured_v2_degen = dut->tile_degenerate_o;
    }
    if (capture_outputs && e.old_tile_done) {
      captured_old_done = true;
      captured_old_crc = dut->old_tile_crc_o;
      captured_old_crc_index = dut->old_tile_crc_index_o;
      captured_old_cov = dut->old_tile_cov_count_o;
      captured_old_degen = dut->old_tile_degenerate_o;
    }

    held_cand = dut->stage_candidate_valid_o && !dut->packet_c_cand_fire_o;
    if (held_cand) held_cand_data = copy_wide(dut->stage_candidate_data_o);
    held_fb = dut->fb_valid_o && !dut->fb_ready_i;
    if (held_fb) {
      held_fb_rgb = dut->fb_rgb565_o;
      held_fb_tag = dut->fb_tag_o;
      held_fb_addr = dut->fb_addr_o;
      held_fb_x = static_cast<int16_t>(dut->fb_x_o);
      held_fb_y = static_cast<int16_t>(dut->fb_y_o);
      held_fb_source = dut->fb_src_id_o;
      held_fb_last = dut->fb_last_o;
    }
    held_old_fb = dut->old_fb_valid_o && !dut->fb_ready_i;
    if (held_old_fb) {
      held_old_fb_rgb = dut->old_fb_rgb565_o;
      held_old_fb_tag = dut->old_fb_tag_o;
      held_old_fb_addr = dut->old_fb_addr_o;
      held_old_fb_x = static_cast<int16_t>(dut->old_fb_x_o);
      held_old_fb_y = static_cast<int16_t>(dut->old_fb_y_o);
      held_old_fb_source = dut->old_fb_src_id_o;
      held_old_fb_last = dut->old_fb_last_o;
    }

    const uint32_t accepted_fill_line = dut->fill_req_addr_o;
    dut->clk = 1;
    dut->eval();
    context->timeInc(1);
    ++g_cycle;

    if (fill_active) {
      if (fill_delay != 0) {
        --fill_delay;
      } else {
        ++fill_beat;
        if (fill_beat == 8) {
          fill_active = false;
          fill_beat = 0;
        }
      }
    }
    if (e.fill_req) {
      require(!fill_active, "texture cache issued overlapping line fills");
      fill_active = true;
      fill_line = accepted_fill_line;
      fill_beat = 0;
      fill_delay = next_fill_delay;
      next_fill_delay = 0;
    }
    return e;
  }

  void reset_capture() {
    captured_v2_rgb.fill(0); captured_v2_tag.fill(0); captured_v2_addr.fill(0);
    captured_v2_x.fill(0); captured_v2_y.fill(0); captured_v2_source.fill(0);
    captured_old_rgb.fill(0); captured_old_tag.fill(0); captured_old_addr.fill(0);
    captured_old_x.fill(0); captured_old_y.fill(0); captured_old_source.fill(0);
    captured_v2_count = captured_old_count = 0;
    captured_v2_done = captured_old_done = false;
    captured_v2_crc = captured_old_crc = 0;
    captured_v2_crc_index = captured_old_crc_index = 0;
    captured_v2_cov = captured_old_cov = 0;
    captured_v2_degen = captured_old_degen = false;
  }

  void reset() {
    park();
    for (unsigned i = 0; i < 4; ++i) step();
    dut->rst_n = 1;
    dut->frame_fault_clear_valid_i = 1;
    for (unsigned i = 0; i < 64; ++i) {
      step();
      require(!dut->binner_initialized_o && !dut->quiet_o &&
                  !dut->frame_fault_clear_ready_o,
              "production-scale binner init falsely exposed quiet/clear-ready");
    }
    dut->frame_fault_clear_valid_i = 0;
    for (unsigned i = 0; i < 1200 && !dut->tri_ready_o; ++i) step();
    require(dut->tri_ready_o, "production-scale binner init never reached tri_ready");
    step();  // latch the real-ready initialization witness
    require(dut->binner_initialized_o && dut->quiet_o &&
                dut->texture_quiet_o && dut->fragment_idle_o,
            "Packet-D did not become quiet after real binner initialization");
    require(!dut->frame_fault_o && !dut->raster_abort_o &&
                dut->candidate_cancel_count_o == 0 &&
                dut->sequence_drop_count_o == 0,
            "Packet-D reset counters/fault levels were not zero");
  }

  void begin_frame(unsigned grid_w, unsigned grid_h, uint64_t clear_word) {
    dut->grid_w_i = grid_w;
    dut->grid_h_i = grid_h;
    dut->frame_clear_word_i = clear_word;
    dut->frame_begin_i = 1;
    step();
    dut->frame_begin_i = 0;
    // Poison the live input after capture; every tile must retain the begin-edge word.
    dut->frame_clear_word_i = ~clear_word;
    for (unsigned guard = 0; guard < 2000 && !dut->tri_ready_o; ++guard) step();
    require(dut->tri_ready_o, "binner did not finish frame-begin clear");
  }

  void drive_job(const Job& job) {
    const Triangle& t = job.tri;
    dut->tri_kx0_i = mask_u64(t.kx[0], 23);
    dut->tri_ky0_i = mask_u64(t.ky[0], 23);
    dut->tri_kc0_i = mask_u64(t.kc[0], 48);
    dut->tri_kx1_i = mask_u64(t.kx[1], 23);
    dut->tri_ky1_i = mask_u64(t.ky[1], 23);
    dut->tri_kc1_i = mask_u64(t.kc[1], 48);
    dut->tri_kx2_i = mask_u64(t.kx[2], 23);
    dut->tri_ky2_i = mask_u64(t.ky[2], 23);
    dut->tri_kc2_i = mask_u64(t.kc[2], 48);
    dut->tri_tl_i = (t.tl[0] ? 1u : 0u) |
                    (t.tl[1] ? 2u : 0u) |
                    (t.tl[2] ? 4u : 0u);
    dut->tri_ax_i = mask_u64(t.ax, 21);
    dut->tri_ay_i = mask_u64(t.ay, 21);
    dut->tri_bx_i = mask_u64(t.bx, 21);
    dut->tri_by_i = mask_u64(t.by, 21);
    dut->tri_cx_i = mask_u64(t.cx, 21);
    dut->tri_cy_i = mask_u64(t.cy, 21);
    dut->tri_min_x_i = mask_u64(t.min_x, 12);
    dut->tri_max_x_i = mask_u64(t.max_x, 12);
    dut->tri_min_y_i = mask_u64(t.min_y, 12);
    dut->tri_max_y_i = mask_u64(t.max_y, 12);
    dut->tri_src_id_i = job.source;
    dut->tri_area2_i = static_cast<uint64_t>(t.area);
    drive_plane(dut->tri_invw_plane_i, job.invw);
    drive_plane(dut->tri_u_over_w_plane_i, job.u);
    drive_plane(dut->tri_v_over_w_plane_i, job.v);

    clear_wide(dut->tri_flat_request_i);
    set_bits(dut->tri_flat_request_i, 0, 8, job.mat.palette_generation);
    set_bits(dut->tri_flat_request_i, 8, 2, job.mat.palette_slot);
    set_bits(dut->tri_flat_request_i, 10, 2, job.mat.response_class);
    set_bits(dut->tri_flat_request_i, 12, 8, job.mat.base_alpha);
    set_bits(dut->tri_flat_request_i, 20, 24, job.mat.base_rgb);
    if (job.mat.aux_context_nonzero)
      set_bits(dut->tri_flat_request_i, 44, 1, 1);
    set_bits(dut->tri_flat_request_i, 268, 1, job.mat.aux_required ? 1 : 0);
    set_bits(dut->tri_flat_request_i, 269, 8, job.mat.recipe_weight);
    set_bits(dut->tri_flat_request_i, 277, 3, job.mat.recipe);
    set_bits(dut->tri_flat_request_i, 280, 8, job.mat.lod);
    set_bits(dut->tri_flat_request_i, 288, 8, job.mat.binding);
    set_bits(dut->tri_flat_request_i, 296, 2, job.mat.sample_count);

    dut->tri_continuation_tail_i =
        (static_cast<uint64_t>(job.mat.vertex_rgb & 0xffffffu) << 24) |
        (static_cast<uint64_t>(job.mat.vertex_alpha) << 16) |
        (static_cast<uint64_t>(job.mat.effect_tag) << 8) |
        job.mat.stencil;
    dut->tri_fragment_state_i = job.mat.state;

    const uint32_t flat_depth = static_cast<uint32_t>(current_rast_attr(
        job.invw, job.tri.area, job.tri.min_x, job.tri.min_x,
        job.tri.min_y, 0, 0));
    dut->old_job_fill_word_i =
        (static_cast<uint64_t>(job.mat.vertex_rgb & 0xffffffu) << 40) |
        (static_cast<uint64_t>(job.mat.effect_tag) << 32) |
        (static_cast<uint64_t>(flat_depth & 0xffffffu) << 8) |
        job.mat.stencil;
    dut->old_job_state_i = job.mat.state;
    dut->old_job_src_a_i = job.mat.vertex_alpha;
    dut->old_job_texel_rgb_i = job.mat.result_rgb;
    dut->old_job_texel_a_i = job.mat.result_alpha;
    dut->old_job_texel_idx_i = job.mat.result_index;
  }

  void submit(const Job& job) {
    drive_job(job);
    dut->tri_valid_i = 1;
    for (unsigned guard = 0; guard < 20000; ++guard) {
      dut->clk = 0;
      dut->eval();
      if (dut->tri_ready_o) {
        step();
        dut->tri_valid_i = 0;
        clear_wide(dut->tri_flat_request_i);
        dut->tri_continuation_tail_i = 0;
        return;
      }
      step();
    }
    fail("triangle offer did not handshake");
  }

  void end_frame() {
    dut->frame_end_i = 1;
    step();
    dut->frame_end_i = 0;
  }

  void wait_drain(unsigned limit = 3000000) {
    const uint32_t base = drain_done_events;
    for (unsigned guard = 0; guard < limit && drain_done_events == base; ++guard)
      step();
    require(drain_done_events == base + 1, "frame binner drain did not complete");
  }

  void wait_quiet(unsigned limit = 3000000) {
    for (unsigned guard = 0; guard < limit; ++guard) {
      dut->clk = 0;
      dut->eval();
      if (dut->quiet_o) {
        // Quiet and tile_done can become true together on RESOLVE's terminal
        // edge; consume that observable pulse before returning on the level.
        step();
        return;
      }
      step();
    }
    fail("full Packet-D composition did not reach quiet");
  }

  void clear_fault() {
    wait_quiet();
    dut->frame_fault_clear_valid_i = 1;
    for (unsigned guard = 0; guard < 20000; ++guard) {
      dut->clk = 0;
      dut->eval();
      const bool fire = dut->frame_fault_clear_ready_o;
      step();
      if (fire) {
        dut->frame_fault_clear_valid_i = 0;
        step();
        require(!dut->frame_fault_o && !dut->raster_abort_o,
                "quiet clear did not clear/rebase Packet-D terminal");
        return;
      }
    }
    fail("quiet frame-fault clear did not handshake");
  }
};

std::array<uint8_t, 10> binding_row_bytes(uint32_t base, uint32_t mode,
                                          uint8_t slot, uint8_t generation,
                                          bool valid, bool present) {
  std::array<uint8_t, 10> bytes{};
  if (!present) return bytes;
  bytes[0] = static_cast<uint8_t>(base);
  bytes[1] = static_cast<uint8_t>(base >> 8);
  bytes[2] = static_cast<uint8_t>(base >> 16);
  bytes[3] = static_cast<uint8_t>(base >> 24);
  bytes[4] = static_cast<uint8_t>(mode);
  bytes[5] = static_cast<uint8_t>(mode >> 8);
  bytes[6] = static_cast<uint8_t>(mode >> 16);
  bytes[7] = static_cast<uint8_t>(mode >> 24);
  const uint16_t high = static_cast<uint16_t>(slot & 3u) |
      (static_cast<uint16_t>(generation) << 2) |
      (static_cast<uint16_t>(valid ? 1u : 0u) << 10);
  bytes[8] = static_cast<uint8_t>(high);
  bytes[9] = static_cast<uint8_t>(high >> 8);
  return bytes;
}

uint32_t crc32_byte(uint32_t crc, uint8_t value) {
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = ((crc ^ (value >> bit)) & 1u) ? ((crc >> 1) ^ 0xedb88320u)
                                         : (crc >> 1);
  return crc;
}

uint32_t binding_crc(uint8_t generation) {
  uint32_t crc = crc32_byte(0xffffffffu, generation);
  for (unsigned selector = 0; selector < 256; ++selector) {
    const bool present = selector == 1;
    const auto bytes = binding_row_bytes(0x00002000u, 0, 0, 1, true, present);
    for (uint8_t byte : bytes) crc = crc32_byte(crc, byte);
  }
  return crc ^ 0xffffffffu;
}

uint8_t binding_command(Harness& h, uint8_t op, uint8_t generation,
                        uint8_t selector, uint32_t base, bool valid,
                        uint32_t crc) {
  h.dut->cfg_valid_i = 1;
  h.dut->cfg_op_i = op;
  h.dut->cfg_page_generation_i = generation;
  h.dut->cfg_selector_i = selector;
  clear_wide(h.dut->cfg_row_i);
  h.dut->cfg_row_i[0] = base;
  h.dut->cfg_row_i[1] = 0;
  h.dut->cfg_row_i[2] = (op == 1)
      ? ((1u << 2) | (valid ? (1u << 10) : 0u))
      : 0u;
  h.dut->cfg_crc32_i = crc;
  bool accepted = false;
  for (unsigned guard = 0; guard < 20000; ++guard) {
    h.dut->clk = 0;
    h.dut->eval();
    const bool fire = h.dut->cfg_valid_i && h.dut->cfg_ready_o;
    h.step();
    if (fire && !accepted) {
      accepted = true;
      h.dut->cfg_valid_i = 0;
    }
    h.dut->clk = 0;
    h.dut->eval();
    if (h.dut->cfg_rsp_valid_o) {
      const uint8_t status = h.dut->cfg_rsp_status_o;
      h.step();
      return status;
    }
  }
  fail("binding command response timed out");
}

void pulse_palette(Harness& h, uint8_t op, uint8_t index, uint16_t rgb565) {
  h.dut->pal_load_valid_i = 1;
  h.dut->pal_load_op_i = op;
  h.dut->pal_load_slot_i = 0;
  h.dut->pal_load_gen_i = 1;
  h.dut->pal_load_idx_i = index;
  h.dut->pal_load_rgb565_i = rgb565;
  h.dut->pal_load_crc_ok_i = 1;
  for (unsigned guard = 0; guard < 2000; ++guard) {
    h.dut->clk = 0;
    h.dut->eval();
    if (h.dut->pal_load_ready_o) {
      h.step();
      h.dut->pal_load_valid_i = 0;
      return;
    }
    h.step();
  }
  fail("palette programming timed out");
}

void program_texture_tables(Harness& h) {
  pulse_palette(h, 0, 0, 0);
  for (unsigned i = 0; i < 256; ++i)
    pulse_palette(h, 1, static_cast<uint8_t>(i), i == 5 ? 0x07e0u : 0u);
  pulse_palette(h, 2, 0, 0);
  require(binding_command(h, 0, 1, 0, 0, false, 0) == 0,
          "binding BEGIN failed");
  require(binding_command(h, 1, 1, 1, 0x00002000u, true, 0) == 0,
          "binding row failed");
  require(binding_command(h, 2, 1, 0, 0, false, binding_crc(1)) == 0,
          "binding END failed");
  require(h.dut->active_page_generation_o == 1,
          "binding generation did not activate");
}

std::vector<ExpectedCandidate> candidates_for(const Job& job,
                                               int tile_x, int tile_y) {
  std::vector<ExpectedCandidate> out;
  for (int row = 0; row < 16; ++row) {
    for (int col = 0; col < 16; ++col) {
      if (!covered(job.tri, tile_x + col, tile_y + row)) continue;
      ExpectedCandidate e;
      e.addr = static_cast<uint8_t>((row << 4) | col);
      e.depth = static_cast<uint32_t>(current_rast_attr(
          job.invw, job.tri.area, job.tri.min_x, tile_x, tile_y, row, col));
      e.u = current_rast_attr(job.u, job.tri.area, job.tri.min_x,
                              tile_x, tile_y, row, col);
      e.v = current_rast_attr(job.v, job.tri.area, job.tri.min_x,
                              tile_x, tile_y, row, col);
      e.state = job.mat.state;
      e.source = job.source;
      e.mat = job.mat;
      out.push_back(e);
    }
  }
  return out;
}

TileOracle build_tile_oracle(const std::vector<Job>& jobs, int tile_x,
                             int tile_y, uint16_t tile_index,
                             uint64_t clear_word) {
  std::array<uint64_t, 256> words{};
  words.fill(clear_word);
  uint32_t coverage_sum = 0;
  for (const Job& job : jobs) {
    const auto candidates = candidates_for(job, tile_x, tile_y);
    coverage_sum += static_cast<uint32_t>(candidates.size());
    for (const ExpectedCandidate& e : candidates) {
      zref::FragmentPipeline::Frag f;
      f.addr = e.addr;
      f.depth = e.depth;
      f.state = e.state;
      f.vr = static_cast<uint8_t>(e.mat.vertex_rgb >> 16);
      f.vg = static_cast<uint8_t>(e.mat.vertex_rgb >> 8);
      f.vb = static_cast<uint8_t>(e.mat.vertex_rgb);
      f.va = e.mat.vertex_alpha;
      f.tag = e.mat.effect_tag;
      f.sten_ref = e.mat.stencil;
      f.tr = static_cast<uint8_t>(e.mat.result_rgb >> 16);
      f.tg = static_cast<uint8_t>(e.mat.result_rgb >> 8);
      f.tb = static_cast<uint8_t>(e.mat.result_rgb);
      f.ta = e.mat.result_alpha;
      f.tidx = e.mat.result_index;
      const auto result = zref::FragmentPipeline::apply(f, words[e.addr]);
      if (result.write) words[e.addr] = result.word;
    }
  }
  TileOracle out;
  out.x = tile_x;
  out.y = tile_y;
  out.index = tile_index;
  out.source = jobs.back().source;
  out.coverage = static_cast<uint16_t>(coverage_sum);
  out.degenerate = false;
  out.resolved = zref::TileResolve::tile(words.data(), tile_x, tile_y);
  return out;
}

zref::TileResolve::Out build_prefix_oracle(const Job& job, int tile_x,
                                            int tile_y, uint64_t clear_word,
                                            std::size_t count) {
  std::array<uint64_t, 256> words{};
  words.fill(clear_word);
  const auto candidates = candidates_for(job, tile_x, tile_y);
  require(count <= candidates.size(), "prefix oracle count exceeded coverage");
  for (std::size_t i = 0; i < count; ++i) {
    const ExpectedCandidate& e = candidates[i];
    zref::FragmentPipeline::Frag f;
    f.addr = e.addr;
    f.depth = e.depth;
    f.state = e.state;
    f.vr = static_cast<uint8_t>(e.mat.vertex_rgb >> 16);
    f.vg = static_cast<uint8_t>(e.mat.vertex_rgb >> 8);
    f.vb = static_cast<uint8_t>(e.mat.vertex_rgb);
    f.va = e.mat.vertex_alpha;
    f.tag = e.mat.effect_tag;
    f.sten_ref = e.mat.stencil;
    f.tr = static_cast<uint8_t>(e.mat.result_rgb >> 16);
    f.tg = static_cast<uint8_t>(e.mat.result_rgb >> 8);
    f.tb = static_cast<uint8_t>(e.mat.result_rgb);
    f.ta = e.mat.result_alpha;
    f.tidx = e.mat.result_index;
    const auto result = zref::FragmentPipeline::apply(f, words[e.addr]);
    if (result.write) words[e.addr] = result.word;
  }
  return zref::TileResolve::tile(words.data(), tile_x, tile_y);
}

Job make_job(const Triangle& tri, uint16_t source, int32_t depth,
             const Material& mat, bool varying_uv = false) {
  Job job;
  job.tri = tri;
  job.source = source;
  job.invw = affine_plane(depth, 0, 0, tri.area);
  job.u = varying_uv ? affine_plane(0x00010000, 2, 3, tri.area)
                     : affine_plane(0, 0, 0, tri.area);
  job.v = varying_uv ? affine_plane(0x00020000, 4, 1, tri.area)
                     : affine_plane(0, 0, 0, tri.area);
  job.mat = mat;
  return job;
}

void append_expected(Harness& h, const Job& job, int tile_x, int tile_y) {
  for (const auto& e : candidates_for(job, tile_x, tile_y))
    h.expected_candidates.push_back(e);
}

void run_flat_old_v2_differential(Harness& h) {
  begin_test("unchanged old/V2 flat bin-to-resolve differential and skewed start");
  const uint64_t clear = make_clear_word(0x102030u, 0x07, 0x010203, 0x09);
  const Triangle a = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  const Triangle b = make_triangle(16, 0, 16, 16, 0, 16, 0, 15, 0, 15);
  Material flat;
  flat.base_rgb = flat.result_rgb = flat.vertex_rgb = 0x336699u;
  flat.vertex_alpha = flat.base_alpha = flat.result_alpha = 0xd5;
  flat.effect_tag = 0x5c;
  flat.stencil = 0xa6;
  flat.state = 0;
  const Job ja = make_job(a, 0x0f01, 0x500000, flat);
  const Job jb = make_job(b, 0x0f02, 0x500000, flat);
  append_expected(h, ja, 0, 0);
  append_expected(h, jb, 0, 0);
  const TileOracle oracle = build_tile_oracle({ja, jb}, 0, 0, 0, clear);
  h.expected_tiles.push_back(oracle);

  h.reset_capture();
  h.capture_outputs = true;
  h.framebuffer_backpressure = true;
  h.dut->old_enable_i = 1;
  h.begin_frame(1, 1, clear);
  h.submit(ja);
  h.submit(jb);
  h.end_frame();

  h.dut->test_start_enable_i = 1;
  bool saw_01 = false, saw_03 = false, saw_07 = false, saw_0f = false;
  const uint32_t drain_base = h.drain_done_events;
  const uint32_t old_drain_base = h.old_drain_done_events;
  for (unsigned guard = 0; guard < 3000000; ++guard) {
    h.step();
    const uint8_t delivered = h.dut->start_delivered_mask_o;
    if (delivered == 0x01) {
      saw_01 = true;
      h.dut->test_start_enable_i = 3;
    } else if (delivered == 0x03) {
      saw_03 = true;
      h.dut->test_start_enable_i = 7;
    } else if (delivered == 0x07) {
      saw_07 = true;
      h.dut->test_start_enable_i = 15;
    } else if (delivered == 0x0f) {
      saw_0f = true;
      h.dut->test_start_enable_i = 31;
    }
    if (h.captured_v2_done && h.captured_old_done &&
        h.drain_done_events == drain_base + 1 &&
        h.old_drain_done_events == old_drain_base + 1)
      break;
  }
  h.wait_quiet();
  require(saw_01 && saw_03 && saw_07 && saw_0f,
          "held start fanout did not expose 01/03/07/0f skew sequence");
  require(h.captured_v2_done && h.captured_old_done &&
              h.captured_v2_count == 256 && h.captured_old_count == 256,
          "old/V2 differential did not complete one 256-beat tile each");
  for (unsigned i = 0; i < 256; ++i) {
    require(h.captured_v2_rgb[i] == h.captured_old_rgb[i] &&
                h.captured_v2_tag[i] == h.captured_old_tag[i] &&
                h.captured_v2_addr[i] == h.captured_old_addr[i] &&
                h.captured_v2_x[i] == h.captured_old_x[i] &&
                h.captured_v2_y[i] == h.captured_old_y[i] &&
                h.captured_v2_source[i] == h.captured_old_source[i],
            "old/V2 flat framebuffer beat differed");
  }
  require(h.captured_v2_crc == h.captured_old_crc &&
              h.captured_v2_crc == oracle.resolved.crc32c &&
              h.captured_v2_crc_index == h.captured_old_crc_index,
          "old/V2 flat CRC/index differed");
  require(h.captured_v2_cov == h.captured_old_cov &&
              h.captured_v2_cov == oracle.coverage &&
              h.captured_v2_degen == h.captured_old_degen &&
              h.captured_v2_degen == oracle.degenerate,
          "old/V2 coverage/degenerate resolve shadows differed");
  require(h.expected_candidates.empty() && h.expected_fragments.empty() &&
              h.expected_tiles.empty(),
          "old/V2 differential left a V2 oracle expectation");
  require(!h.dut->frame_fault_o && !h.dut->fragment_error_o,
          "old/V2 flat differential raised an unrelated fault");
  h.dut->old_enable_i = 0;
  h.capture_outputs = false;
  h.framebuffer_backpressure = false;
}

void run_healthy_scene(Harness& h) {
  begin_test("metadata, delivered-mask skew, three planes, CLUT raw index, accumulation");
  const uint64_t clear = make_clear_word(0x000000u, 0, 0, 0);
  const Triangle a = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  const Triangle b = make_triangle(16, 0, 16, 16, 0, 16, 0, 15, 0, 15);

  Material red;
  red.base_rgb = red.result_rgb = 0xff0000u;
  red.vertex_rgb = 0xff0000u;
  red.effect_tag = 0x31;
  red.stencil = 0x41;

  Material green;
  green.sample_count = 1;
  green.binding = 1;
  green.palette_generation = 1;
  green.base_rgb = 0;
  green.result_rgb = 0x00ff00u;
  green.result_index = 5;
  green.vertex_rgb = 0xffffffu;
  green.effect_tag = 0x32;
  green.stencil = 0x42;
  green.state = zref::FragmentPipeline::star_disc_masked().pack();

  const Job ja = make_job(a, 0x1101, 0x400000, red, true);
  const Job jb = make_job(b, 0x1102, 0x800000, green, false);
  append_expected(h, ja, 0, 0);
  append_expected(h, jb, 0, 0);
  h.expected_tiles.push_back(build_tile_oracle({ja, jb}, 0, 0, 0, clear));

  const uint32_t resolved_base = h.dut->resolved_tiles_o;
  const uint32_t jobs_base = h.dut->raster_jobs_started_o;
  const uint32_t binner_base = h.dut->binner_tile_references_o;
  const uint32_t taken_base = h.dut->jobs_taken_o;
  const uint32_t drain_base = h.drain_done_events;
  const bool front_base = h.dut->front_bank_o;
  h.framebuffer_backpressure = true;
  h.begin_frame(1, 1, clear);
  h.submit(ja);
  h.submit(jb);
  h.end_frame();

  // Hold lane 1 and lane 2 off for the first real row, then release one at a
  // time.  The held row may retire only after masks 001 -> 011 -> 111.
  h.dut->test_attr_cov_enable_i = 1;
  bool saw_001 = false, saw_011 = false;
  for (unsigned guard = 0; guard < 3000000 &&
       h.drain_done_events == drain_base; ++guard) {
    h.step();
    if (h.dut->coverage_hold_valid_o &&
        h.dut->coverage_delivered_mask_o == 1) {
      saw_001 = true;
      h.dut->test_attr_cov_enable_i = 3;
    } else if (h.dut->coverage_hold_valid_o &&
               h.dut->coverage_delivered_mask_o == 3) {
      saw_011 = true;
      h.dut->test_attr_cov_enable_i = 7;
    }
  }
  require(h.drain_done_events == drain_base + 1, "healthy frame did not drain");
  h.wait_quiet();
  require(saw_001 && saw_011,
          "coverage delivered mask did not expose exact skewed 001/011 states");
  require(h.expected_candidates.empty() && h.expected_fragments.empty(),
          "healthy frame left candidate/fragment expectations unconsumed");
  if (!h.expected_tiles.empty()) {
    std::fprintf(stderr,
        "packet-d diag tiles=%zu done=%u resolved=%u abort=%u local=%u seq=%u "
        "fault=%u quiet=%u texq=%u fridle=%u skid=%u jobs=%u cand=%u frag=%u drops=%u\n",
        h.expected_tiles.size(), h.tile_done_events, h.dut->resolved_tiles_o,
        h.dut->raster_abort_o, h.dut->local_attribute_abort_o,
        h.dut->sequence_abort_o, h.dut->frame_fault_o, h.dut->quiet_o,
        h.dut->texture_quiet_o, h.dut->fragment_idle_o, h.dut->skid_level_o,
        h.dut->raster_jobs_started_o, h.cand_fires, h.fragment_fires, h.drop_fires);
  }
  require(h.expected_tiles.empty(), "healthy frame left tile oracle unresolved");
  require(h.dut->binner_tile_references_o == binner_base + 2 &&
              h.dut->jobs_taken_o == taken_base + 2 &&
              h.dut->raster_jobs_started_o == jobs_base + 2,
          "multi-triangle binner/raster job counts were not exactly two");
  require(h.dut->resolved_tiles_o == resolved_base + 1 &&
              static_cast<bool>(h.dut->front_bank_o) != front_base,
          "two triangles did not produce exactly one swap/resolve");
  require(h.saw_green_raw_five,
          "one-sample CLUT result lost raw index 5 or palette green");
  require(!h.saw_sheet_request && !h.dut->frame_fault_o &&
              !h.dut->fragment_error_o,
          "healthy selected profile issued AUX or raised a pipeline fault");
  h.framebuffer_backpressure = false;
}

void run_varying_depth_earlyz_scene(Harness& h) {
  begin_test("varying invw depth and exact Early-Z accept/reject census");
  const uint64_t clear = make_clear_word(0, 0, 0, 0);
  const Triangle a = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  const Triangle b = make_triangle(16, 0, 16, 16, 0, 16, 0, 15, 0, 15);
  Material high;
  high.base_rgb = high.result_rgb = high.vertex_rgb = 0x8080ffu;
  high.effect_tag = 0x61;
  Material low = high;
  low.vertex_rgb = low.base_rgb = low.result_rgb = 0xff4000u;
  low.effect_tag = 0x62;
  low.state = 1u;  // exact late/Early-Z strict depth test enabled

  Job j0 = make_job(a, 0x2a01, 0x700000, high);
  j0.invw = affine_plane(0x700000, 2, 3, a.area);
  const Job j1 = make_job(b, 0x2a02, 0x710000, high);
  const Job j2 = make_job(a, 0x2a03, 0x100000, low);
  const auto c0 = candidates_for(j0, 0, 0);
  const auto c1 = candidates_for(j1, 0, 0);
  const auto rejected = candidates_for(j2, 0, 0);
  require(c0.size() + c1.size() == 256 && rejected.size() == 136,
          "directed Early-Z coverage fixture lost its exact partition");
  for (const auto& e : c0) h.expected_candidates.push_back(e);
  for (const auto& e : c1) h.expected_candidates.push_back(e);
  h.expected_tiles.push_back(build_tile_oracle({j0, j1, j2}, 0, 0, 0, clear));

  const uint32_t reject_base = h.dut->early_z_rejects_o;
  const uint32_t covered_base = h.dut->early_z_covered_o;
  const uint32_t cand_base = h.cand_fires;
  const uint32_t fragment_base = h.fragment_fires;
  const uint32_t drain_base = h.drain_done_events;
  h.begin_frame(1, 1, clear);
  h.submit(j0);
  h.submit(j1);
  h.submit(j2);
  h.end_frame();
  for (unsigned guard = 0; guard < 3000000 &&
       h.drain_done_events == drain_base; ++guard)
    h.step();
  require(h.drain_done_events == drain_base + 1,
          "varying-depth frame did not drain exactly once");
  h.wait_quiet();
  require(h.dut->early_z_rejects_o == reject_base + rejected.size() &&
              h.dut->early_z_covered_o == covered_base + 392,
          "Early-Z accept/reject census differed from varying-depth oracle");
  require(h.cand_fires == cand_base + 256 &&
              h.fragment_fires == fragment_base + 256,
          "Early-Z survivor count was not exactly the full high-depth cover");
  require(h.dut->z_floor_o > 0x100000 &&
              h.expected_candidates.empty() && h.expected_fragments.empty() &&
              h.expected_tiles.empty(),
          "varying-depth frame lost floor promotion or oracle retirement");
  require(!h.dut->frame_fault_o && !h.dut->fragment_error_o,
          "varying-depth Early-Z frame raised an unrelated fault");
}

void run_overlap_scene(Harness& h) {
  begin_test("next-front rendering overlaps previous resolve under framebuffer stalls");
  const uint64_t clear = make_clear_word(0x010203u, 0x01, 0, 0);
  const Triangle t0 = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  const Triangle t1 = make_triangle(16, 0, 32, 0, 16, 16, 16, 31, 0, 15);
  Material blue;
  blue.base_rgb = blue.result_rgb = blue.vertex_rgb = 0x0000ffu;
  blue.effect_tag = 0x51;
  Material yellow;
  yellow.base_rgb = yellow.result_rgb = yellow.vertex_rgb = 0xffff00u;
  yellow.effect_tag = 0x52;
  const Job j0 = make_job(t0, 0x2201, 0x500000, blue);
  const Job j1 = make_job(t1, 0x2202, 0x600000, yellow);
  append_expected(h, j0, 0, 0);
  append_expected(h, j1, 16, 0);
  h.expected_tiles.push_back(build_tile_oracle({j0}, 0, 0, 0, clear));
  h.expected_tiles.push_back(build_tile_oracle({j1}, 16, 0, 1, clear));

  const uint32_t started_base = h.dut->raster_jobs_started_o;
  const uint32_t done_base = h.tile_done_events;
  h.framebuffer_backpressure = true;
  h.begin_frame(2, 1, clear);
  h.submit(j0);
  h.submit(j1);
  h.end_frame();
  const uint32_t drain_base = h.drain_done_events;
  for (unsigned guard = 0; guard < 3000000 && h.drain_done_events == drain_base; ++guard) {
    h.step();
    if (h.dut->raster_jobs_started_o >= started_base + 2 &&
        h.tile_done_events == done_base)
      h.saw_second_job_before_first_done = true;
  }
  require(h.drain_done_events == drain_base + 1, "overlap frame did not drain");
  h.wait_quiet();
  require(h.saw_second_job_before_first_done,
          "next-front job did not begin before previous resolve completed");
  require(h.tile_done_events == done_base + 2 && h.expected_tiles.empty(),
          "overlap frame did not resolve exactly two oracle tiles");
  require(h.expected_candidates.empty() && h.expected_fragments.empty(),
          "overlap frame left candidate/fragment expectations");
  h.framebuffer_backpressure = false;
}

void run_local_faults(Harness& h) {
  begin_test("hostile AUX and signed invw range terminate with zero leakage");
  const Triangle t = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  Material aux;
  aux.base_rgb = aux.result_rgb = aux.vertex_rgb = 0xffffffu;
  aux.aux_required = true;
  const Job aux_job = make_job(t, 0x3301, 0x700000, aux);
  const uint32_t aux_base = h.dut->aux_profile_fault_count_o;
  const uint32_t range_before_aux = h.dut->range_fault_count_o;
  const uint32_t coord_before_aux = h.dut->coordinate_fault_count_o;
  const uint32_t local_before_aux = h.dut->local_fault_count_o;
  const uint32_t local_drop_before_aux = h.dut->local_drop_count_o;
  const uint32_t cancel_before_aux = h.dut->candidate_cancel_count_o;
  const uint32_t cand_before_aux = h.cand_fires;
  const uint32_t frag_before_aux = h.fragment_fires;
  const uint32_t fb_before_aux = h.fb_fires;
  const uint32_t tiles_before_aux = h.tile_done_events;
  const uint32_t resolved_before_aux = h.dut->resolved_tiles_o;
  const uint32_t store_before_aux = h.dut->tilestore_references_o;
  const uint32_t seqdrop_before_aux = h.dut->sequence_drop_count_o;
  const bool front_before_aux = h.dut->front_bank_o;
  h.begin_frame(1, 1, 0);
  h.submit(aux_job);
  h.end_frame();
  h.wait_drain();
  h.wait_quiet();
  require(h.dut->raster_abort_o && h.dut->local_attribute_abort_o &&
              h.dut->aux_profile_fault_count_o == aux_base + 1 &&
              h.dut->local_fault_count_o == local_before_aux + 1 &&
              h.dut->range_fault_count_o == range_before_aux &&
              h.dut->coordinate_fault_count_o == coord_before_aux,
          "hostile AUX lacked its exact isolated local profile signature");
  require(h.cand_fires == cand_before_aux && h.fragment_fires == frag_before_aux &&
              h.fb_fires == fb_before_aux && h.tile_done_events == tiles_before_aux &&
              h.dut->resolved_tiles_o == resolved_before_aux &&
              h.dut->tilestore_references_o == store_before_aux &&
              static_cast<bool>(h.dut->front_bank_o) == front_before_aux &&
              h.dut->local_drop_count_o == local_drop_before_aux &&
              h.dut->candidate_cancel_count_o == cancel_before_aux &&
              h.dut->sequence_drop_count_o == seqdrop_before_aux &&
              !h.dut->sequence_abort_o && !h.dut->fragment_error_o &&
              !h.saw_sheet_request,
          "hostile AUX leaked candidate/fragment/write/fb/swap/resolve work");
  h.clear_fault();

  Material range;
  range.base_rgb = range.result_rgb = range.vertex_rgb = 0xffffffu;
  Job bad = make_job(t, 0x3302, -1, range);
  const uint32_t range_base = h.dut->range_fault_count_o;
  const uint32_t aux_before_range = h.dut->aux_profile_fault_count_o;
  const uint32_t coord_before_range = h.dut->coordinate_fault_count_o;
  const uint32_t local_base = h.dut->local_fault_count_o;
  const uint32_t drops_base = h.dut->local_drop_count_o;
  const uint32_t cancels_base = h.dut->candidate_cancel_count_o;
  const uint32_t cand_base = h.cand_fires;
  const uint32_t frag_base = h.fragment_fires;
  const uint32_t fb_base = h.fb_fires;
  const uint32_t tile_base = h.tile_done_events;
  const uint32_t resolved_base = h.dut->resolved_tiles_o;
  const uint32_t store_base = h.dut->tilestore_references_o;
  const bool front_base = h.dut->front_bank_o;
  h.begin_frame(1, 1, 0);
  h.submit(bad);
  h.end_frame();
  h.wait_drain();
  h.wait_quiet();
  require(h.dut->raster_abort_o &&
              h.dut->range_fault_count_o == range_base + 1 &&
              h.dut->local_fault_count_o == local_base + 1 &&
              h.dut->aux_profile_fault_count_o == aux_before_range &&
              h.dut->coordinate_fault_count_o == coord_before_range,
          "negative invw lacked its exact isolated range signature");
  require(h.dut->local_drop_count_o == drops_base + 136 &&
              h.dut->candidate_cancel_count_o == cancels_base,
          "negative invw did not count every joined abort-drain bundle exactly");
  require(h.cand_fires == cand_base && h.fragment_fires == frag_base &&
              h.fb_fires == fb_base && h.tile_done_events == tile_base &&
              h.dut->resolved_tiles_o == resolved_base &&
              h.dut->tilestore_references_o == store_base &&
              static_cast<bool>(h.dut->front_bank_o) == front_base &&
              !h.dut->sequence_abort_o && !h.dut->fragment_error_o,
          "negative invw leaked candidate/fragment/write/fb/swap/resolve work");
  h.clear_fault();
}

void run_healthy() {
  auto* h = new Harness;
  h->reset();
  program_texture_tables(*h);
  run_flat_old_v2_differential(*h);
  run_healthy_scene(*h);
  run_varying_depth_earlyz_scene(*h);
  run_overlap_scene(*h);
  run_local_faults(*h);
  require(h->dut->quiet_o && h->dut->attribute_idle_o == 7 &&
              h->dut->skid_level_o == 0 && h->dut->texture_quiet_o &&
              h->dut->fragment_idle_o,
          "healthy gate ended without complete structural quiet");
}

Job simple_abort_job(uint16_t source = 0x4401) {
  const Triangle t = make_triangle(0, 0, 16, 0, 0, 16, 0, 15, 0, 15);
  Material m;
  m.base_rgb = m.result_rgb = m.vertex_rgb = 0x808080u;
  return make_job(t, source, 0x600000, m, true);
}

void run_coordinate_mutant() {
  auto* h = new Harness;
  h->reset();
  begin_test("coordinate mutant fires finite local abort with exact two-drop edge");
  const bool front_base = h->dut->front_bank_o;
  h->dut->test_stage_admit_enable_i = 0;
  h->begin_frame(1, 1, 0);
  h->submit(simple_abort_job());
  h->end_frame();
  h->wait_drain();
  h->wait_quiet();
  require(h->dut->coordinate_fault_count_o == 1 &&
              h->dut->local_fault_count_o == 1 && h->dut->raster_abort_o &&
              h->dut->range_fault_count_o == 0 &&
              h->dut->aux_profile_fault_count_o == 0,
          "coordinate mutant lacked exact isolated local diagnostic signature");
  require(h->saw_simultaneous_attr_earlyz_drop &&
              h->dut->candidate_cancel_count_o == 2 &&
              h->dut->local_drop_count_o == 134,
          "coordinate mutant missed exact simultaneous/later local drop accounting");
  require(h->cand_fires == 0 && h->fragment_fires == 0 && h->fb_fires == 0 &&
              h->tile_done_events == 0 && h->dut->resolved_tiles_o == 0 &&
              h->dut->tilestore_references_o == 0 &&
              static_cast<bool>(h->dut->front_bank_o) == front_base &&
              h->dut->attribute_idle_o == 7 && !h->dut->sequence_abort_o &&
              h->dut->sequence_drop_count_o == 0 && !h->dut->fragment_error_o,
          "coordinate mutant leaked work or changed an unrelated detector");
  std::printf("Packet-D coordinate mutant FIRED\n");
}

void run_omit_quiet_mutant() {
  auto* h = new Harness;
  h->reset();
  program_texture_tables(*h);
  h->validate_framebuffer = false;
  begin_test("omit-V3-quiet mutant produces exact bounded partial-tile corruption");
  Job job = simple_abort_job(0x5501);
  job.mat.sample_count = 1;
  job.mat.binding = 1;
  job.mat.palette_generation = 1;
  job.mat.result_rgb = 0x00ff00u;
  job.mat.result_index = 5;
  job.mat.state = zref::FragmentPipeline::star_disc_masked().pack();
  const auto all_candidates = candidates_for(job, 0, 0);
  append_expected(*h, job, 0, 0);
  const TileOracle correct = build_tile_oracle({job}, 0, 0, 0, 0);
  const bool front_base = h->dut->front_bank_o;
  const uint32_t frag_base = h->fragment_fires;
  const uint32_t drain_base = h->drain_done_events;
  const uint32_t tile_base = h->tile_done_events;
  h->reset_capture();
  h->capture_outputs = true;
  h->next_fill_delay = 500;
  h->begin_frame(1, 1, 0);
  h->submit(job);
  h->end_frame();
  std::size_t prefix_count = all_candidates.size();
  for (unsigned guard = 0; guard < 3000000; ++guard) {
    h->step();
    if (static_cast<bool>(h->dut->front_bank_o) != front_base &&
        !h->dut->texture_quiet_o) {
      h->saw_early_swap = true;
      prefix_count = h->fragment_fires - frag_base;
      break;
    }
  }
  require(h->saw_early_swap && prefix_count < all_candidates.size() &&
              !h->dut->raster_abort_o,
          "omit-V3-quiet mutant lacked non-vacuous early partial swap");
  for (unsigned guard = 0; guard < 3000000 &&
       (!h->captured_v2_done || h->drain_done_events == drain_base ||
        !h->dut->texture_quiet_o || !h->dut->fragment_idle_o); ++guard)
    h->step();
  h->wait_quiet();
  const auto partial = build_prefix_oracle(job, 0, 0, 0, prefix_count);
  if (!(h->captured_v2_done && h->captured_v2_count == 256 &&
        h->tile_done_events == tile_base + 1 &&
        h->drain_done_events == drain_base + 1)) {
    std::fprintf(stderr,
        "omit diag captured_done=%u captured_count=%u tile_done=%u base=%u "
        "drain=%u base=%u texq=%u fridle=%u quiet=%u prefix=%zu/%zu\n",
        h->captured_v2_done, h->captured_v2_count, h->tile_done_events, tile_base,
        h->drain_done_events, drain_base, h->dut->texture_quiet_o,
        h->dut->fragment_idle_o, h->dut->quiet_o, prefix_count,
        all_candidates.size());
  }
  require(h->captured_v2_done && h->captured_v2_count == 256 &&
              h->tile_done_events == tile_base + 1 &&
              h->drain_done_events == drain_base + 1,
          "omit-V3-quiet mutant did not reach bounded post-corruption drain");
  for (unsigned i = 0; i < 256; ++i) {
    require(h->captured_v2_rgb[i] == partial.rgb565[i] &&
                h->captured_v2_tag[i] == partial.tag[i] &&
                h->captured_v2_addr[i] == i &&
                h->captured_v2_x[i] == static_cast<int16_t>(i & 15u) &&
                h->captured_v2_y[i] == static_cast<int16_t>(i >> 4) &&
                h->captured_v2_source[i] == job.source,
            "omit-V3-quiet corruption differed from exact pre-swap fragment prefix");
  }
  require(h->captured_v2_crc == partial.crc32c &&
              h->captured_v2_crc != correct.resolved.crc32c &&
              h->captured_v2_cov == correct.coverage && !h->captured_v2_degen,
          "omit-V3-quiet CRC/cov signature was not the intended partial tile");
  require(h->expected_candidates.empty() && h->expected_fragments.empty() &&
              h->dut->local_fault_count_o == 0 &&
              h->dut->coordinate_fault_count_o == 0 &&
              h->dut->range_fault_count_o == 0 &&
              h->dut->aux_profile_fault_count_o == 0 &&
              h->dut->candidate_cancel_count_o == 0 &&
              h->dut->local_drop_count_o == 0 &&
              !h->dut->sequence_abort_o && h->dut->sequence_drop_count_o == 0 &&
              !h->dut->fragment_error_o && !h->dut->frame_fault_o,
          "omit-V3-quiet mutant changed an unrelated fault/counter");
  std::printf("Packet-D omit-V3-quiet mutant FIRED\n");
}

void require_sequence_unrelated_clean(Harness& h) {
  require(h.dut->local_fault_count_o == 0 &&
              h.dut->coordinate_fault_count_o == 0 &&
              h.dut->range_fault_count_o == 0 &&
              h.dut->aux_profile_fault_count_o == 0 &&
              !h.dut->local_attribute_abort_o && !h.dut->fragment_error_o,
          "sequence mutant changed an unrelated local/fragment detector");
}

void run_sequence_mutant(bool old_ready, bool skip_cancel) {
  auto* h = new Harness;
  h->reset();
  h->validate_candidates = h->validate_fragments = h->validate_framebuffer = false;
  begin_test(old_ready ? "old-ready identity strands V3 head"
                       : (skip_cancel ? "skip-cancel identity strands skid head"
                                      : "identity abort cancels skid and reaches quiet"));
  const uint32_t tile_base = h->tile_done_events;
  const bool front_base = h->dut->front_bank_o;
  h->begin_frame(1, 1, 0);
  h->submit(simple_abort_job(0x6601));
  h->submit(simple_abort_job(0x6602));
  h->end_frame();

  bool gate_closed = false;
  bool mismatch = false;
  uint32_t skid_at_mismatch = 0;
  uint32_t cancels_at_mismatch = 0;
  uint32_t fragments_at_mismatch = 0;
  for (unsigned guard = 0; guard < 3000000; ++guard) {
    if (!gate_closed && h->dut->admission_sequence_o >= 4) {
      h->dut->test_stage_admit_enable_i = 0;
      gate_closed = true;
    }
    const auto e = h->step();
    if (e.mismatch) {
      mismatch = true;
      skid_at_mismatch = h->dut->skid_level_o;
      cancels_at_mismatch = h->dut->candidate_cancel_count_o;
      fragments_at_mismatch = h->fragment_fires;
      break;
    }
  }
  require(gate_closed && mismatch && skid_at_mismatch != 0 &&
              fragments_at_mismatch != 0,
          "identity control did not fire after an earlier fragment with occupied skid work");
  h->dut->test_stage_admit_enable_i = 1;

  if (old_ready) {
    for (unsigned i = 0; i < 4000; ++i) h->step();
    require(h->dut->sequence_abort_o && !h->dut->texture_quiet_o &&
                !h->dut->quiet_o && h->dut->sequence_drop_count_o == 0,
            "old-ready mutant did not strand the mismatched ordered head");
    require(h->fragment_fires == fragments_at_mismatch &&
                h->tile_done_events == tile_base &&
                static_cast<bool>(h->dut->front_bank_o) == front_base,
            "old-ready mutant leaked a later fragment or started a swap/resolve");
    require(h->drain_done_events == 1 && h->dut->binner_tile_references_o == 2 &&
                h->dut->jobs_taken_o == 2 && h->dut->raster_jobs_started_o == 1 &&
                h->dut->raster_jobs_sunk_o == 1,
            "old-ready abort did not sink the exact remaining binner job");
    require(h->dut->candidate_cancel_count_o ==
                skid_at_mismatch + cancels_at_mismatch &&
                h->dut->local_drop_count_o != 0,
            "old-ready abort lacked exact skid cancellation/attr drain evidence");
    require_sequence_unrelated_clean(*h);
    std::printf("Packet-D old-ready mutant FIRED\n");
    return;
  }

  if (skip_cancel) {
    for (unsigned i = 0; i < 20000; ++i) h->step();
    require(h->dut->sequence_abort_o && h->dut->texture_quiet_o &&
                h->dut->fragment_idle_o && h->dut->skid_level_o != 0 &&
                !h->dut->quiet_o && h->dut->candidate_cancel_count_o == 0,
            "skip-cancel mutant did not leave occupied skid/no quiet");
    require(h->fragment_fires == fragments_at_mismatch &&
                h->tile_done_events == tile_base &&
                static_cast<bool>(h->dut->front_bank_o) == front_base,
            "skip-cancel mutant leaked a later fragment or started a swap/resolve");
    require(h->drain_done_events == 1 && h->dut->binner_tile_references_o == 2 &&
                h->dut->jobs_taken_o == 2 && h->dut->raster_jobs_started_o == 1 &&
                h->dut->raster_jobs_sunk_o == 1,
            "skip-cancel abort did not sink the exact remaining binner job");
    require(h->dut->local_drop_count_o != 0,
            "skip-cancel abort lacked joined-attribute drain accounting");
    require_sequence_unrelated_clean(*h);
    std::printf("Packet-D skip-cancel mutant FIRED\n");
    return;
  }

  for (unsigned guard = 0; guard < 20000 && h->drain_done_events == 0; ++guard)
    h->step();
  require(h->drain_done_events == 1 && h->dut->binner_tile_references_o == 2 &&
              h->dut->jobs_taken_o == 2 && h->dut->raster_jobs_started_o == 1 &&
              h->dut->raster_jobs_sunk_o == 1,
          "identity abort did not sink the exact remaining binner job");
  h->wait_quiet();
  require(h->dut->sequence_abort_o && h->dut->raster_abort_o &&
              h->dut->candidate_cancel_count_o ==
                  skid_at_mismatch + cancels_at_mismatch &&
              h->dut->skid_level_o == 0,
          "identity abort did not cancel the exact occupied skid census");
  require(h->dut->admission_sequence_o ==
              h->fragment_fires + h->dut->sequence_drop_count_o &&
              h->drop_fires == h->dut->sequence_drop_count_o,
          "identity accounting violated S=F+SD");
  require(h->fragment_fires == fragments_at_mismatch &&
              h->tile_done_events == tile_base &&
              static_cast<bool>(h->dut->front_bank_o) == front_base &&
              h->dut->attribute_idle_o == 7 && h->dut->texture_quiet_o &&
              h->dut->fragment_idle_o && h->dut->local_drop_count_o != 0,
          "identity abort leaked later work or lacked full producer drain");
  require_sequence_unrelated_clean(*h);
  const uint32_t admission = h->dut->admission_sequence_o;
  const uint32_t drops = h->dut->sequence_drop_count_o;
  h->clear_fault();
  require(h->dut->expected_sequence_o == admission &&
              h->dut->sequence_drop_count_o == drops,
          "quiet clear did not rebase sequence or preserve monotonic drops");
  std::printf("Packet-D identity/cancel control FIRED\n");
}

}  // namespace

double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#if defined(PACKET_D_EXPECT_COORDINATE)
  run_coordinate_mutant();
#elif defined(PACKET_D_EXPECT_OMIT_V3_QUIET)
  run_omit_quiet_mutant();
#elif defined(PACKET_D_EXPECT_OLD_READY)
  run_sequence_mutant(true, false);
#elif defined(PACKET_D_EXPECT_SKIP_CANCEL)
  run_sequence_mutant(false, true);
#elif defined(PACKET_D_EXPECT_IDENTITY_ABORT)
  run_sequence_mutant(false, false);
#else
  run_healthy();
#endif
  std::printf("packet-d directed PASS tests=%u checks=%u cycles=%llu\n",
              g_tests, g_checks, static_cast<unsigned long long>(g_cycle));
  std::fflush(stdout);
  std::fflush(stderr);
  std::_Exit(0);  // Deliberate hard exit: do not enter Windows model teardown.

  // Implicit `return 0` deadlocks the same way -- see zhao_sim.hpp.
  zhao::exit_hard(0);
}
