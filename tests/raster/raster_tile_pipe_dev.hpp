// raster_tile_pipe_dev.hpp — Verilator driver for the composed phase-4/5 tile
// pipe: RASTER.EDGEWALK -> RASTER.EARLYZ -> RASTER.FRAGMENT ->
// RASTER.TILESTORE -> RASTER.RESOLVE
// (fpga/rtl/raster/zhao_raster_tile_pipe.sv).
//
// THE ORACLE IS THE FIVE ORACLES, WIRED THE SAME WAY. `pipe_oracle()` calls
// zref::EdgeWalk for the coverage, runs every covered pixel through
// zref::EarlyZ and then zref::FragmentPipeline against the destination word
// zref::TileStore currently holds, drives that store with the SAME cycle
// sequence the RTL drives (clear the front bank, one full-word write per
// SURVIVING fragment, swap), and hands the swapped-out bank to
// zref::TileResolve. Every one of those is a view onto a frozen reference or
// a plainly-written contract model — the §8 fill law (rast.cpp), the §8
// strict depth test and unit8 arithmetic (qformats, through zref::unit_mul),
// the charter §8 store contract, and the charter §8 ordered dither
// (resolve.cpp). So "RTL == oracle" is literally "RTL == the five laws,
// composed"; this file contains no fill rule, no blend, no dither, no CRC and
// no word layout of its own.
//
// The ONE thing it does state is the composition's own law 3 — the surface
// pixel address, `fb_x = tile_x + col`, `fb_y = tile_y + row` — because no
// existing reference emits one. It is stated ONCE, in `pipe_address_errors`
// below, and both the directed and the random lane call it.
//
// This header deliberately does not include raster_dev.hpp /
// raster_resolve_dev.hpp: those declare their own `kTile`, `Word` and
// `describe` in namespace zhao_raster, so a TU that pulled in two of them
// would not compile. Names here are prefixed `Pipe`/`kPipe`.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

// THE ORACLE HALF OF THIS HEADER IS USEFUL WITHOUT THE MODEL.
//
// `PipeJob`, `PipeTile`, `PipeExpect` and `pipe_oracle` are views onto the
// frozen reference blocks and know nothing about Verilator. A COMPOSITION that
// contains the tile pipe -- the geometry->raster seam, say -- needs exactly
// those and cannot have this model, because its own top is a different module
// and only one is generated per target.
//
// Defining ZHAO_PIPE_DEV_ORACLE_ONLY takes the oracle and leaves the driver.
// The alternative was copying the oracle into the composition, which is how
// two versions of an expectation start disagreeing.
#ifndef ZHAO_PIPE_DEV_ORACLE_ONLY
#include "Vzhao_raster_tile_pipe.h"
#endif

#include "zhao_sim.hpp"
#include "zref/zref_earlyz.hpp"
#include "zref/zref_edgewalk.hpp"
#include "zref/zref_fragment.hpp"
#include "zref/zref_tileresolve.hpp"
#include "zref/zref_tilestore.hpp"

namespace zhao_raster {

using PipeTri = zref::EdgeWalk::Tri;
using PipeWord = zref::TileStore::Word;

inline constexpr int kPipeTile = zref::TileStore::kTile;
inline constexpr int kPipePixels = zref::TileStore::kWords;

/**
 * One job: a triangle, a tile, the flat fragment source and a clear word.
 *
 * `fill` is the fragment SOURCE in RASTER.TILESTORE's word layout ([63:40]
 * RGB, [39:32] tag, [31:8] depth, [7:0] stencil reference). `state` is the
 * 32-bit fragment state word; **state 0 is the plain opaque write**, so a job
 * that leaves it default writes `fill` at every covered pixel exactly as the
 * pre-RASTER.FRAGMENT composition did. The texel fields are what TEXTURE.TMU
 * would have sampled — that block does not exist, and this driver presents
 * the texel rather than pretending to sample one.
 */
struct PipeJob {
  PipeTri tri;
  int32_t tx = 0;
  int32_t ty = 0;
  uint64_t fill = 0;
  uint64_t clear = 0;
  uint16_t index = 0;
  uint16_t src = 0;
  uint32_t state = 0;
  uint8_t src_a = 0;
  uint32_t texel_rgb = 0;
  uint8_t texel_a = 0;
  uint8_t texel_idx = 0;
};

/** One resolved tile as the composition emits it. */
struct PipeTile {
  uint16_t rgb565[kPipePixels] = {};
  uint8_t tag[kPipePixels] = {};
  int32_t x[kPipePixels] = {};  // SURFACE pixel coordinate of each beat
  int32_t y[kPipePixels] = {};
  uint32_t crc32c = 0;
  uint16_t crc_index = 0;
  uint32_t count = 0;       // covered pixels reported with THIS tile
  bool degenerate = false;  // zero-area reject reported with THIS tile

  bool same_picture(const PipeTile& o) const {
    if (crc32c != o.crc32c) return false;
    for (int i = 0; i < kPipePixels; ++i)
      if (rgb565[i] != o.rgb565[i] || tag[i] != o.tag[i]) return false;
    return true;
  }
};

/** What the five oracles, composed, say the tile must be. */
struct PipeExpect {
  zref::TileResolve::Out res;
  uint32_t count = 0;  // covered pixels (RASTER.EDGEWALK's)
  bool degenerate = false;
  uint32_t rejects = 0;     // early-Z rejects in THIS job
  uint32_t candidates = 0;  // fragments RASTER.EARLYZ passed on
  uint32_t writes = 0;      // fragments that survived all three tests
  uint32_t blended = 0;     // surviving fragments that combined src with dst
};

/** The job's flat fragment source, unpacked into the shading packet. */
inline zref::FragmentPipeline::Frag pipe_frag(const PipeJob& j, uint8_t addr) {
  const PipeWord src = PipeWord::unpack(j.fill);
  zref::FragmentPipeline::Frag f;
  f.addr = addr;
  f.depth = src.depth;
  f.state = j.state;
  f.vr = src.r;
  f.vg = src.g;
  f.vb = src.b;
  f.va = j.src_a;
  f.tag = src.tag;
  f.sten_ref = src.stencil;
  f.tr = static_cast<uint8_t>(j.texel_rgb >> 16);
  f.tg = static_cast<uint8_t>(j.texel_rgb >> 8);
  f.tb = static_cast<uint8_t>(j.texel_rgb);
  f.ta = j.texel_a;
  f.tidx = j.texel_idx;
  return f;
}

/**
 * The composed oracle. `store` and `ez` are carried ACROSS jobs on purpose.
 * The RTL's ping-pong alternates banks tile by tile, so driving the reference
 * store through the identical clear/write/swap sequence means the reference
 * walks the same alternation — a composition that cleared or wrote the wrong
 * bank would diverge from this, not merely from a flat array. zref::EarlyZ is
 * carried for the same reason in the other direction: its counters accumulate
 * across the batch exactly as the RTL's do, so a `tile_begin` that failed to
 * reset the depth floor would show up as a divergence on the SECOND tile
 * rather than being hidden by a fresh model per job.
 */
// ONE LIFECYCLE PER TILE. `first` clears the bank and begins the tile in
// early-Z; `last` swaps and resolves. Both default to true, which is the
// one-clear-one-triangle-one-resolve shape the block test drives and which this
// oracle described exclusively until GEOM.BINNER started saying where a
// reference sits in its tile's list.
//
// A tile with several triangles calls this once per triangle with first on the
// first and last on the last, and the returned PipeExpect is only meaningful
// for the LAST one -- the earlier calls advance the store and the early-Z state
// so the later triangles see what their predecessors wrote.
inline PipeExpect pipe_oracle(zref::TileStore& store, zref::EarlyZ& ez, const PipeJob& j,
                              bool first = true, bool last = true) {
  PipeExpect e;
  const zref::EdgeWalk::Cov cov = zref::EdgeWalk::tile(j.tri, j.tx, j.ty);
  e.count = cov.count;
  e.degenerate = cov.degenerate;

  if (first) {
    zref::TileStore::Cycle c;
    c.clear = true;
    c.clear_data = j.clear;
    store.step(c);
    // Same cycle, same event: the tile now holds the clear word everywhere, so
    // the clear word's DEPTH field is exactly the early-Z floor.
    ez.tile_begin(PipeWord::unpack(j.clear).depth);
  }

  const int front = store.front();
  for (int row = 0; row < kPipeTile; ++row) {
    for (int col = 0; col < kPipeTile; ++col) {
      if (!cov.covered(col, row)) continue;
      const uint8_t addr = static_cast<uint8_t>(row * kPipeTile + col);

      // RASTER.EARLYZ first — it never touches the store.
      if (!ez.fragment(addr, PipeWord::unpack(j.fill).depth, j.state).keep) {
        ++e.rejects;
        continue;
      }
      ++e.candidates;

      // RASTER.FRAGMENT reads the destination the store currently holds,
      // which is why the writes below must be applied as they happen: two
      // fragments at one pixel must see each other.
      const zref::FragmentPipeline::Out fo =
          zref::FragmentPipeline::apply(pipe_frag(j, addr), store.peek(front, addr));
      if (!fo.write) continue;
      ++e.writes;
      if (fo.blended) ++e.blended;

      zref::TileStore::Cycle w;
      w.wr = true;
      w.wr_addr = addr;
      w.wr_data = fo.word;
      store.step(w);
    }
  }

  // An earlier triangle of a tile does not swap and does not resolve; its
  // pixels stay in the front bank for the next one to read.
  if (!last) return e;

  const int written = store.front();  // the bank the swap is about to retire
  zref::TileStore::Cycle s;
  s.swap = true;
  store.step(s);

  // Value-initialised: the loop below fills every element, but cppcheck cannot
  // see that through store.peek() and the static-analysis tier is a hard gate.
  uint64_t words[kPipePixels] = {};
  for (int i = 0; i < kPipePixels; ++i) words[i] = store.peek(written, static_cast<uint8_t>(i));
  e.res = zref::TileResolve::tile(words, j.tx, j.ty);
  return e;
}

/** How the driver feeds jobs — the difference the ping-pong has to show. */
enum class PipeFeed {
  kBackToBack,  // job_valid_i held high continuously: no idle cycles
  kSerial,      // the next job is offered only after the previous tile_done_o
  kGapped       // job_valid_i PCG-gated (feed_seed)
};

#ifndef ZHAO_PIPE_DEV_ORACLE_ONLY
class PipeDev {
 public:
  PipeDev() { reset(); }
  ~PipeDev() { top_.final(); }
  PipeDev(const PipeDev&) = delete;
  PipeDev& operator=(const PipeDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  /**
   * Runs a whole batch of jobs through one instance with no intervening
   * reset, and returns the cycle count from the first offered job to the last
   * `tile_done_o` pulse. `fb_seed` != 0 PCG-gates `fb_ready_i` (backpressure);
   * `feed_seed` is used by PipeFeed::kGapped. Protocol violations land in
   * *err (empty = clean).
   */
  uint32_t run(const std::vector<PipeJob>& jobs, PipeFeed feed, uint32_t feed_seed,
               uint32_t fb_seed, std::vector<PipeTile>* out, std::string* err) {
    reset();
    out->assign(jobs.size(), PipeTile{});
    front_toggles_ = 0;
    max_in_flight_ = 0;
    frag_error_ = false;

    size_t issued = 0;
    size_t done = 0;
    uint32_t emit = 0;  // pixels accepted for the tile currently emitting
    uint32_t rngf = feed_seed ? feed_seed : 1u;
    uint32_t rngb = fb_seed ? fb_seed : 1u;
    bool offering = false;
    bool held = false;
    uint64_t held_pack = 0;
    bool prev_front = false;
    bool have_prev_front = false;
    uint32_t cycles = 0;
    const uint32_t limit = static_cast<uint32_t>(jobs.size()) * 4096u + 8192u;

    park();
    while (done < jobs.size()) {
      if (cycles > limit) {
        add(err, "the pipe never completed the batch");
        break;
      }

      // ---- stimulus ------------------------------------------------------
      if (!offering && issued < jobs.size()) {
        bool want = true;
        if (feed == PipeFeed::kSerial) want = (issued == done);
        if (feed == PipeFeed::kGapped) want = ((next(&rngf) & 3u) != 0u);
        offering = want;
      }
      top_.job_valid_i = offering ? 1 : 0;
      if (offering) drive_job(jobs[issued]);
      const bool fb_rdy = (fb_seed == 0u) || ((next(&rngb) & 3u) != 0u);
      top_.fb_ready_i = fb_rdy ? 1 : 0;
      top_.eval();

      const bool job_acc = offering && (top_.job_ready_o != 0);

      // ---- the framebuffer stream ----------------------------------------
      if (top_.fb_valid_o) {
        if (done >= jobs.size()) {
          add(err, "a framebuffer beat arrived after the last tile completed");
        } else {
          const uint64_t pack = (static_cast<uint64_t>(top_.fb_last_o) << 56) |
                                (static_cast<uint64_t>(top_.fb_addr_o) << 48) |
                                (static_cast<uint64_t>(top_.fb_tag_o) << 40) |
                                (static_cast<uint64_t>(top_.fb_rgb565_o) << 24) |
                                (static_cast<uint64_t>(top_.fb_x_o & 0xFFFu) << 12) |
                                static_cast<uint64_t>(top_.fb_y_o & 0xFFFu);
          if (held && pack != held_pack) add(err, "a stalled fb beat changed while fb_ready_i low");
          if (top_.fb_src_id_o != jobs[done].src) add(err, "fb_src_id_o is not this tile's");
          if (fb_rdy) {
            const uint32_t a = top_.fb_addr_o;
            if (a != emit) add(err, "fb beats are not in tile raster order");
            if (top_.fb_last_o != (emit == static_cast<uint32_t>(kPipePixels) - 1))
              add(err, "fb_last_o is not the 256th pixel");
            if (a < static_cast<uint32_t>(kPipePixels)) {
              PipeTile& t = (*out)[done];
              t.rgb565[a] = top_.fb_rgb565_o;
              t.tag[a] = top_.fb_tag_o;
              t.x[a] = sext12(top_.fb_x_o);
              t.y[a] = sext12(top_.fb_y_o);
            }
            ++emit;
            held = false;
          } else {
            held = true;
            held_pack = pack;
          }
        }
      } else {
        if (held) add(err, "fb_valid_o dropped while a beat was stalled");
        held = false;
      }

      // ---- tile completion -------------------------------------------------
      if (top_.tile_done_o) {
        if (done >= jobs.size()) {
          add(err, "more tiles completed than were issued");
        } else {
          if (emit != static_cast<uint32_t>(kPipePixels))
            add(err, "tile_done_o before all 256 pixels were accepted");
          PipeTile& t = (*out)[done];
          t.crc32c = top_.tile_crc_o;
          t.crc_index = top_.tile_crc_index_o;
          t.count = top_.tile_cov_count_o;
          t.degenerate = top_.tile_degenerate_o != 0;
          if (t.crc_index != jobs[done].index) add(err, "tile_crc_index_o is not this tile's");
          ++done;
          emit = 0;
        }
      }

      // ---- RASTER.FRAGMENT's error pulse ------------------------------------
      // It fires only if RASTER.TILESTORE breaks its own `latency: fixed:1`,
      // so it must never fire. Latched rather than sampled at the end: it is
      // a one-cycle pulse.
      if (top_.fragment_error_o) {
        frag_error_ = true;
        add(err, "fragment_error_o fired: the tile store missed a read response");
      }

      // ---- ping-pong observability ------------------------------------------
      const bool front = top_.front_bank_o != 0;
      if (have_prev_front && front != prev_front) ++front_toggles_;
      prev_front = front;
      have_prev_front = true;

      const size_t in_flight = issued - done;
      if (in_flight > max_in_flight_) max_in_flight_ = in_flight;
      if (in_flight > 2) add(err, "more than two tiles in flight (the ping-pong is only 2 deep)");

      edge();
      ++cycles;
      if (job_acc) {
        ++issued;
        offering = false;
      }
    }

    park();
    top_.eval();
    return cycles;
  }

  /** front_bank_o transitions observed during the last run (1 per tile). */
  uint32_t front_toggles() const { return front_toggles_; }
  /** The deepest issued-minus-completed the last run reached. */
  size_t max_in_flight() const { return max_in_flight_; }
  uint32_t tile_references() const { return top_.tile_references_o; }
  uint32_t resolved_tiles() const { return top_.resolved_tiles_o; }
  uint32_t early_z_rejects() const { return top_.early_z_rejects_o; }
  uint32_t ez_covered() const { return top_.ez_covered_o; }
  uint32_t fr_covered() const { return top_.fr_covered_o; }
  uint32_t blended_fragments() const { return top_.blended_fragments_o; }
  uint8_t bin_mask() const { return static_cast<uint8_t>(top_.bin_mask_o); }
  uint32_t z_floor() const { return top_.z_floor_o; }
  /** RASTER.FRAGMENT's error pulse: it must NEVER fire. */
  bool saw_fragment_error() const { return frag_error_; }

 private:
  static int32_t sext12(uint32_t v) {
    v &= 0xFFFu;
    return (v & 0x800u) ? static_cast<int32_t>(v | 0xFFFFF000u) : static_cast<int32_t>(v);
  }
  static uint32_t mask21(int32_t v) { return static_cast<uint32_t>(v) & 0x1FFFFFu; }
  static uint32_t mask12(int32_t v) { return static_cast<uint32_t>(v) & 0xFFFu; }
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
    return (w >> 22) ^ w;
  }
  static void add(std::string* err, const char* what) {
    if (err->empty()) *err = what;
  }

  void drive_job(const PipeJob& j) {
    top_.job_ax_i = mask21(j.tri.ax);
    top_.job_ay_i = mask21(j.tri.ay);
    top_.job_bx_i = mask21(j.tri.bx);
    top_.job_by_i = mask21(j.tri.by);
    top_.job_cx_i = mask21(j.tri.cx);
    top_.job_cy_i = mask21(j.tri.cy);
    // ONE TRIANGLE PER TILE, WHICH IS WHAT THIS BLOCK TEST DRIVES. `job_first_i`
    // and `job_last_i` let a caller accumulate several triangles into one tile
    // before resolving; a caller that asserts BOTH gets exactly the
    // one-clear-one-triangle-one-resolve shape this file has always tested, so
    // every check below is unchanged and still means what it did.
    //
    // The COMPOSED test drives them properly: tests/render/render_pipe_directed
    // hands the pipe a real tile list from GEOM.BINNER.
    top_.job_first_i = 1;
    top_.job_last_i = 1;
    top_.job_tile_x_i = mask12(j.tx);
    top_.job_tile_y_i = mask12(j.ty);
    top_.job_fill_word_i = j.fill;
    top_.job_clear_word_i = j.clear;
    top_.job_tile_index_i = j.index;
    top_.job_src_id_i = j.src;
    top_.job_state_i = j.state;
    top_.job_src_a_i = j.src_a;
    top_.job_texel_rgb_i = j.texel_rgb;
    top_.job_texel_a_i = j.texel_a;
    top_.job_texel_idx_i = j.texel_idx;
  }

  void park() {
    top_.job_valid_i = 0;
    top_.fb_ready_i = 1;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_tile_pipe top_;
  uint32_t front_toggles_ = 0;
  size_t max_in_flight_ = 0;
  bool frag_error_ = false;
};
#endif  // ZHAO_PIPE_DEV_ORACLE_ONLY

// ---------------------------------------------------------------- helpers ---
inline uint64_t pipe_word(uint8_t r, uint8_t g, uint8_t b, uint8_t tag = 0, uint32_t depth = 0,
                          uint8_t stencil = 0) {
  PipeWord w;
  w.r = r;
  w.g = g;
  w.b = b;
  w.tag = tag;
  w.depth = depth;
  w.stencil = stencil;
  return w.pack();
}

/** Pixels, in S 12.8 screen subpixels (spec/qformats.md §8). */
inline int32_t px(int32_t p) { return p * 256; }

/** Pack a 24-bit texel RGB for PipeJob::texel_rgb. */
inline uint32_t pipe_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

inline bool pipe_match(const PipeExpect& want, const PipeTile& got) {
  if (want.res.crc32c != got.crc32c) return false;
  if (want.count != got.count || want.degenerate != got.degenerate) return false;
  for (int i = 0; i < kPipePixels; ++i)
    if (want.res.rgb565[i] != got.rgb565[i] || want.res.tag[i] != got.tag[i]) return false;
  return true;
}

/**
 * The composition's own law 3, stated once: every framebuffer beat carries the
 * SURFACE coordinate of its pixel — the resolving tile's origin plus its
 * in-tile {col, row}, wrapping in the signed 12-bit pixel space the job came
 * in on. Returns the number of beats that do not. No existing reference emits
 * a framebuffer address, so this is the one law the test tree states rather
 * than delegates.
 */
inline uint32_t pipe_address_errors(const PipeJob& j, const PipeTile& t) {
  auto sext12 = [](int32_t v) {
    const uint32_t u = static_cast<uint32_t>(v) & 0xFFFu;
    return (u & 0x800u) ? static_cast<int32_t>(u | 0xFFFFF000u) : static_cast<int32_t>(u);
  };
  uint32_t wrong = 0;
  for (int i = 0; i < kPipePixels; ++i) {
    if (t.x[i] != sext12(j.tx + (i % kPipeTile))) ++wrong;
    if (t.y[i] != sext12(j.ty + (i / kPipeTile))) ++wrong;
  }
  return wrong;
}

inline std::string pipe_describe(const PipeJob& j, const PipeExpect& want, const PipeTile& got) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "tri (%d,%d)(%d,%d)(%d,%d) tile (%d,%d)", j.tri.ax, j.tri.ay,
                j.tri.bx, j.tri.by, j.tri.cx, j.tri.cy, j.tx, j.ty);
  std::string s(buf);
  int shown = 0;
  for (int i = 0; i < kPipePixels && shown < 8; ++i) {
    if (want.res.rgb565[i] == got.rgb565[i] && want.res.tag[i] == got.tag[i]) continue;
    std::snprintf(buf, sizeof(buf), "\n  px %3d (row %2d col %2d): oracle %04X/%02X rtl %04X/%02X",
                  i, i / kPipeTile, i % kPipeTile, want.res.rgb565[i], want.res.tag[i],
                  got.rgb565[i], got.tag[i]);
    s += buf;
    ++shown;
  }
  std::snprintf(buf, sizeof(buf), "\n  crc oracle %08X rtl %08X; count %u/%u; degenerate %d/%d",
                want.res.crc32c, got.crc32c, want.count, got.count,
                static_cast<int>(want.degenerate), static_cast<int>(got.degenerate));
  s += buf;
  return s;
}

inline std::vector<uint8_t> pipe_serialize(const PipeJob& j) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  auto put64 = [&put32](uint64_t x) {
    put32(static_cast<uint32_t>(x));
    put32(static_cast<uint32_t>(x >> 32));
  };
  put32(static_cast<uint32_t>(j.tri.ax));
  put32(static_cast<uint32_t>(j.tri.ay));
  put32(static_cast<uint32_t>(j.tri.bx));
  put32(static_cast<uint32_t>(j.tri.by));
  put32(static_cast<uint32_t>(j.tri.cx));
  put32(static_cast<uint32_t>(j.tri.cy));
  put32(static_cast<uint32_t>(j.tx));
  put32(static_cast<uint32_t>(j.ty));
  put64(j.fill);
  put64(j.clear);
  return v;
}

}  // namespace zhao_raster
