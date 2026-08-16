// debug_crc_directed.cpp — DEBUG.CRC directed vectors (plan W2.6 /
// design/contracts/DEBUG.CRC.md "Directed tests").
//
//   1. known-vector streams: the capture_format.md 2.1 CRC-32C test vectors
//      replayed as displayed streams (the oracle is zhao_abi::zhao_crc32c —
//      the same machine as the frame packet CRC, plan A3d)
//   2. mode-dependent byte counts: full displayed canvases for all three
//      modes (184,320 / 153,600 / 245,760 B) — PCG pixels, oracle bit-exact
//   3. repeat law: the SAME canvas twice -> identical CRC (the 60 Hz law's
//      mechanical proof, spec/video_rules.md 4); one flipped byte differs
//   4. mis-sized stream: wrong byte count -> size_err event, CRC NOT
//      published; bytes outside any frame -> same violation path

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"
#include "Vzhao_debug_crc.h"

#include "zhao_sim.hpp"
#include "zref/zref.hpp"
#include "zref/zref_cmd2.hpp"

using zhao::check;

namespace {

struct Pcg32 {
  uint64_t state;
  uint64_t inc;
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ull + inc;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18) ^ old) >> 27);
    const uint32_t rot = static_cast<uint32_t>(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }
  uint32_t operator()(uint32_t bound) { return next() % bound; }
};

class CrcDev {
 public:
  CrcDev() : top_(new Vzhao_debug_crc) { reset(); }
  ~CrcDev() { top_->final(); delete top_; }
  CrcDev(const CrcDev&) = delete;
  CrcDev& operator=(const CrcDev&) = delete;

  void reset() {
    top_->rst_n = 0;
    park();
    top_->eval();
    for (int i = 0; i < 2; ++i) edge();
    top_->rst_n = 1;
    top_->eval();
  }

  void park() {
    top_->in_valid_i = 0;
    top_->in_byte_i = 0;
    top_->in_sof_i = 0;
    top_->in_eof_i = 0;
    top_->expect_bytes_i = 0;
  }

  // stream one displayed frame; returns {crc, valid, err}
  struct Result {
    uint32_t crc;
    bool valid;
    bool err;
  };
  Result stream(const std::vector<uint8_t>& bytes, uint32_t expect) {
    Result r{0, false, false};
    size_t i = 0;
    while (i < bytes.size()) {
      top_->in_valid_i = 1;
      top_->in_byte_i = bytes[i];
      top_->in_sof_i = (i == 0) ? 1 : 0;
      top_->in_eof_i = (i + 1 == bytes.size()) ? 1 : 0;
      top_->expect_bytes_i = expect;
      top_->eval();
      edge();
      if (top_->frame_crc_valid_o) {
        r.crc = top_->frame_crc_o;
        r.valid = true;
      }
      if (top_->size_err_evt_o) r.err = true;
      ++i;
    }
    top_->in_valid_i = 0;
    top_->in_sof_i = 0;
    top_->in_eof_i = 0;
    top_->eval();
    edge();  // the finalize pulse lands the cycle after the eof byte
    if (top_->frame_crc_valid_o) {
      r.crc = top_->frame_crc_o;
      r.valid = true;
    }
    if (top_->size_err_evt_o) r.err = true;
    return r;
  }

  Vzhao_debug_crc* top_;

 private:
  void edge() {
    top_->clk = 0;
    top_->eval();
    top_->clk = 1;
    top_->eval();
    top_->clk = 0;
    top_->eval();
  }
};

}  // namespace

// ---- random lane: PCG displayed streams vs the ABI CRC machine -----------
static int runRandom(uint32_t frames, uint64_t seed) {
  Pcg32 rng{seed, (seed << 1) | 1u};
  CrcDev t;
  for (uint32_t f = 0; f < frames; ++f) {
    const uint32_t n = 1 + rng(4096);
    std::vector<uint8_t> bytes(n);
    for (auto& b : bytes) b = static_cast<uint8_t>(rng.next());
    const uint32_t want = zhao_abi::zhao_crc32c(0, bytes.data(), n);
    const CrcDev::Result r = t.stream(bytes, n);
    check(r.valid && !r.err, "rand: valid frame", 1, r.valid);
    check(r.crc == want, "rand: oracle bit-exact", want, r.crc);
    zref::Crc32c model;
    const zref::Crc32c::Result m = model.frame(bytes, n);
    check(m.valid && !m.size_err && m.crc == r.crc,
          "rand: zref::Crc32c device oracle agrees", m.crc, r.crc);
  }
  std::printf("debug_crc random: %u frames\n", frames);
  return 0;
}

int main(int argc, char** argv) {
  uint32_t random_frames = 0;
  uint64_t seed = 0xC2CC2C20260815ull;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_frames = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    }
  }
  if (random_frames > 0) {
    runRandom(random_frames, seed);
    return zhao::report_and_exit("debug_crc_random");
  }

  // ---- 1. known vectors (capture_format.md 2.1) -----------------------------
  {
    CrcDev t;
    struct Vec {
      const char* name;
      std::vector<uint8_t> bytes;
      uint32_t crc;
    };
    const std::vector<uint8_t> zeros(32, 0x00);
    const std::vector<uint8_t> ones(32, 0xFF);
    std::vector<uint8_t> ramp, ramp_r;
    for (int i = 0; i < 32; ++i) ramp.push_back(static_cast<uint8_t>(i));
    for (int i = 31; i >= 0; --i) ramp_r.push_back(static_cast<uint8_t>(i));
    const std::vector<Vec> vecs = {
        {"empty-ish \"123456789\"",
         {'1', '2', '3', '4', '5', '6', '7', '8', '9'}, 0xE3069283u},
        {"32 x 00", zeros, 0x8A9136AAu},
        {"32 x FF", ones, 0x62A8AB43u},
        {"32 B 0x00..0x1F", ramp, 0x46DD794Eu},
        {"32 B 0x1F..0x00", ramp_r, 0x113FDB5Cu},
    };
    for (const Vec& v : vecs) {
      const CrcDev::Result r = t.stream(v.bytes, static_cast<uint32_t>(v.bytes.size()));
      check(r.valid, v.name, 1, r.valid);
      check(!r.err, std::string(v.name).append(" (size ok)").c_str(), 0, r.err);
      check(r.crc == v.crc, std::string(v.name).append(" (crc)").c_str(), v.crc,
            r.crc);
      const uint32_t orc =
          zhao_abi::zhao_crc32c(0, v.bytes.data(), v.bytes.size());
      check(r.crc == orc, std::string(v.name).append(" (== oracle)").c_str(), orc,
            r.crc);
      zref::Crc32c model;
      const zref::Crc32c::Result m =
          model.frame(v.bytes, static_cast<uint32_t>(v.bytes.size()));
      check(m.valid && m.crc == r.crc,
            std::string(v.name).append(" (== zref::Crc32c)").c_str(), m.crc,
            r.crc);
    }
  }

  // ---- 2. full canvases per mode (byte-count law) ----------------------------
  {
    CrcDev t;
    struct Mode {
      const char* name;
      uint32_t bytes;
    };
    const Mode modes[] = {
        {"Z60", 184320}, {"Storm", 153600}, {"Duo", 245760},
    };
    for (const Mode& m : modes) {
      Pcg32 rng{0xC0FFEE12u + m.bytes, 0x9E3779B9u};
      std::vector<uint8_t> canvas(m.bytes);
      for (uint32_t i = 0; i < m.bytes; ++i) {
        canvas[i] = static_cast<uint8_t>(rng.next());
      }
      const uint32_t want = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
      const CrcDev::Result r = t.stream(canvas, m.bytes);
      check(r.valid && !r.err, std::string(m.name).append(": valid").c_str(), 1,
            r.valid);
      check(r.crc == want, std::string(m.name).append(": oracle bit-exact").c_str(),
            want, r.crc);
      zref::Crc32c model;
      const zref::Crc32c::Result mr = model.frame(canvas, m.bytes);
      check(mr.valid && mr.crc == r.crc,
            std::string(m.name).append(": zref::Crc32c agrees").c_str(), mr.crc,
            r.crc);
    }
  }

  // ---- 3. repeat law ----------------------------------------------------------
  {
    CrcDev t;
    Pcg32 rng{0xD0D0D0D0u, 0x1234567u};
    std::vector<uint8_t> a(64 * 1024);
    for (auto& b : a) b = static_cast<uint8_t>(rng.next());
    std::vector<uint8_t> b = a;                 // the SAME frame repeated
    const uint32_t ea = static_cast<uint32_t>(a.size());
    const CrcDev::Result ra = t.stream(a, ea);
    const CrcDev::Result rb = t.stream(b, ea);
    check(ra.valid && rb.valid, "repeat: both valid", 1, ra.valid && rb.valid);
    check(ra.crc == rb.crc, "repeat: repeated frame CRCs identical (60 Hz law)",
          ra.crc, rb.crc);
    b[b.size() / 2] ^= 0x01;                    // one flipped byte
    const CrcDev::Result rc = t.stream(b, ea);
    check(rc.crc != ra.crc, "repeat: a changed frame CRCs differently", ra.crc,
          rc.crc);
  }

  // ---- 4. mis-sized stream + stray bytes --------------------------------------
  {
    CrcDev t;
    std::vector<uint8_t> canvas(1024, 0x5A);
    // eof 4 bytes early: mis-sized -> error, no published CRC
    CrcDev::Result r = t.stream(std::vector<uint8_t>(canvas.begin(), canvas.end() - 4),
                                1024);
    check(r.err, "size: mis-sized stream flagged", 1, r.err);
    check(!r.valid, "size: CRC not published", 0, r.valid);
    zref::Crc32c model;
    const zref::Crc32c::Result m = model.frame(
        std::vector<uint8_t>(canvas.begin(), canvas.end() - 4), 1024);
    check(m.size_err && !m.valid, "size: zref::Crc32c agrees (err, no publish)",
          1, m.size_err);

    // a byte outside any frame (no sof since idle): raster violation
    t.park();
    t.top_->in_valid_i = 1;
    t.top_->in_byte_i = 0x42;
    t.top_->in_sof_i = 0;
    t.top_->in_eof_i = 0;
    t.top_->expect_bytes_i = 1;
    t.top_->eval();
    t.top_->clk = 1;
    t.top_->eval();
    t.top_->clk = 0;
    t.top_->eval();
    check(t.top_->size_err_evt_o != 0, "size: stray byte flagged", 1,
          t.top_->size_err_evt_o);
  }

  return zhao::report_and_exit("debug_crc_directed");
}
