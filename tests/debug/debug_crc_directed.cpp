// debug_crc_directed.cpp — DEBUG.CRC directed vectors (plan W2.6 /
// design/contracts/DEBUG.CRC.md "Directed tests").
//
// THE DIFFERENTIAL IS CROSS-GRANULARITY, and that is deliberate. Since
// 2026-08-22 the block lives in vid_clk and consumes ONE DISPLAYED PIXEL per
// clock; the oracle it is measured against — the SHIPPED `zref::Crc32c` and
// the generated `zhao_abi::zhao_crc32c` — still consumes BYTES, one at a
// time, and neither was changed for this test. So every case here states the
// displayed stream as bytes, hands those bytes to the oracle unchanged, packs
// the same bytes into RGB565 pixels (low byte first, video_rules.md §3) for
// the device, and demands bit-exact agreement. A byte-order slip inside the
// pixel lane cannot pass: the oracle would be folding the two bytes in the
// other order.
//
//   1. known-vector streams: the capture_format.md §2.1 CRC-32C test vectors
//      replayed as displayed streams. The canonical `"123456789"` vector is
//      NOT here — it is nine bytes, and a pixel-granular stream cannot be an
//      odd number of bytes long. It is checked against the generated machine
//      in tests/unit/test_crc.cpp and tests/fuzz/test_abi_fuzz_parity.cpp;
//      what this file owns is the device's framing and publish law, not the
//      polynomial.
//   2. mode-dependent byte counts: full displayed canvases for all three
//      modes (184,320 / 153,600 / 245,760 B) — PCG pixels, oracle bit-exact
//   3. repeat law: the SAME canvas twice -> identical CRC (the 60 Hz law's
//      mechanical proof, spec/video_rules.md §4); one flipped byte differs
//   4. mis-sized stream: wrong byte count -> size_err event, CRC NOT
//      published; pixels outside any frame -> same violation path; and an
//      ODD expectation, which a pixel-granular stream can never satisfy
//   5. byte order and endianness, stated as its own case rather than left
//      implicit in the random lane
//   6. bytes_captured_o — the length the last event reported

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

// The displayed stream as the serializer presents it: RGB565 little-endian
// halfwords (video_rules.md §3), so byte 2i is the LOW byte of pixel i.
std::vector<uint16_t> pack_pixels(const std::vector<uint8_t>& bytes) {
  std::vector<uint16_t> px(bytes.size() / 2);
  for (size_t i = 0; i < px.size(); ++i) {
    px[i] = static_cast<uint16_t>(bytes[2 * i] | (bytes[2 * i + 1] << 8));
  }
  return px;
}

class CrcDev {
 public:
  CrcDev() : top_(new Vzhao_debug_crc) { reset(); }
  ~CrcDev() {
    top_->final();
    delete top_;
  }
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
    top_->in_px_i = 0;
    top_->in_sof_i = 0;
    top_->in_eof_i = 0;
    top_->expect_bytes_i = 0;
  }

  struct Result {
    uint32_t crc;
    bool valid;
    bool err;
    uint32_t bytes_captured;
  };

  // stream one displayed frame, one PIXEL per clock. `expect_after_sof`, when
  // given, is presented on every cycle EXCEPT the sof one — the device must
  // have LATCHED the expectation at sof and must ignore whatever arrives
  // afterwards (contract: "restarts the CRC, latches expect_bytes_i").
  Result streamPx(const std::vector<uint16_t>& px, uint32_t expect,
                  uint32_t expect_after_sof = 0, bool vary_expect = false) {
    Result r{0, false, false, 0};
    size_t i = 0;
    while (i < px.size()) {
      top_->in_valid_i = 1;
      top_->in_px_i = px[i];
      top_->in_sof_i = (i == 0) ? 1 : 0;
      top_->in_eof_i = (i + 1 == px.size()) ? 1 : 0;
      top_->expect_bytes_i = (vary_expect && i != 0) ? expect_after_sof : expect;
      top_->eval();
      edge();
      collect(r);
      ++i;
    }
    park();
    top_->eval();
    edge();  // the finalize pulse lands the cycle after the eof pixel
    collect(r);
    return r;
  }

  // stream one displayed frame stated as BYTES (must be an even count — a
  // pixel-granular stream has no odd length)
  Result stream(const std::vector<uint8_t>& bytes, uint32_t expect) {
    return streamPx(pack_pixels(bytes), expect);
  }

  Vzhao_debug_crc* top_;

 private:
  void collect(Result& r) {
    if (top_->frame_crc_valid_o) {
      r.crc = top_->frame_crc_o;
      r.valid = true;
      r.bytes_captured = top_->bytes_captured_o;
    }
    if (top_->size_err_evt_o) {
      r.err = true;
      r.bytes_captured = top_->bytes_captured_o;
    }
  }

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
    const uint32_t npx = 1 + rng(2048);
    std::vector<uint8_t> bytes(size_t{npx} * 2);
    for (auto& b : bytes) b = static_cast<uint8_t>(rng.next());
    const uint32_t nb = static_cast<uint32_t>(bytes.size());
    const uint32_t want = zhao_abi::zhao_crc32c(0, bytes.data(), nb);
    const CrcDev::Result r = t.stream(bytes, nb);
    check(r.valid && !r.err, "rand: valid frame", 1, r.valid);
    check(r.crc == want, "rand: oracle bit-exact", want, r.crc);
    check(r.bytes_captured == nb, "rand: bytes captured", nb, r.bytes_captured);
    zref::Crc32c model;
    const zref::Crc32c::Result m = model.frame(bytes, nb);
    check(m.valid && !m.size_err && m.crc == r.crc, "rand: zref::Crc32c device oracle agrees",
          m.crc, r.crc);

    // and the same stream against a WRONG expectation, chosen so it can never
    // be met: the device must flag it and publish nothing, and the shipped
    // oracle must say the same
    const uint32_t bad = nb + 1 + rng(7);
    const CrcDev::Result rb = t.stream(bytes, bad);
    check(rb.err && !rb.valid, "rand: mis-sized flagged, nothing published", 1, rb.err);
    check(rb.bytes_captured == nb, "rand: mis-sized length reported", nb, rb.bytes_captured);
    zref::Crc32c bad_model;
    const zref::Crc32c::Result bm = bad_model.frame(bytes, bad);
    check(bm.size_err && !bm.valid, "rand: zref::Crc32c agrees on mis-sized", 1, bm.size_err);
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

  // ---- 1. known vectors (capture_format.md §2.1) ----------------------------
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
        {"32 x 00", zeros, 0x8A9136AAu},
        {"32 x FF", ones, 0x62A8AB43u},
        {"32 B 0x00..0x1F", ramp, 0x46DD794Eu},
        {"32 B 0x1F..0x00", ramp_r, 0x113FDB5Cu},
    };
    for (const Vec& v : vecs) {
      const CrcDev::Result r = t.stream(v.bytes, static_cast<uint32_t>(v.bytes.size()));
      check(r.valid, v.name, 1, r.valid);
      check(!r.err, std::string(v.name).append(" (size ok)").c_str(), 0, r.err);
      check(r.crc == v.crc, std::string(v.name).append(" (crc)").c_str(), v.crc, r.crc);
      const uint32_t orc = zhao_abi::zhao_crc32c(0, v.bytes.data(), v.bytes.size());
      check(r.crc == orc, std::string(v.name).append(" (== oracle)").c_str(), orc, r.crc);
      zref::Crc32c model;
      const zref::Crc32c::Result m = model.frame(v.bytes, static_cast<uint32_t>(v.bytes.size()));
      check(m.valid && m.crc == r.crc, std::string(v.name).append(" (== zref::Crc32c)").c_str(),
            m.crc, r.crc);
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
        {"Z60", 184320},
        {"Storm", 153600},
        {"Duo", 245760},
    };
    for (const Mode& m : modes) {
      Pcg32 rng{0xC0FFEE12u + m.bytes, 0x9E3779B9u};
      std::vector<uint8_t> canvas(m.bytes);
      for (uint32_t i = 0; i < m.bytes; ++i) {
        canvas[i] = static_cast<uint8_t>(rng.next());
      }
      const uint32_t want = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
      const CrcDev::Result r = t.stream(canvas, m.bytes);
      check(r.valid && !r.err, std::string(m.name).append(": valid").c_str(), 1, r.valid);
      check(r.crc == want, std::string(m.name).append(": oracle bit-exact").c_str(), want, r.crc);
      check(r.bytes_captured == m.bytes, std::string(m.name).append(": bytes captured").c_str(),
            m.bytes, r.bytes_captured);
      zref::Crc32c model;
      const zref::Crc32c::Result mr = model.frame(canvas, m.bytes);
      check(mr.valid && mr.crc == r.crc,
            std::string(m.name).append(": zref::Crc32c agrees").c_str(), mr.crc, r.crc);
    }
  }

  // ---- 3. repeat law ----------------------------------------------------------
  {
    CrcDev t;
    Pcg32 rng{0xD0D0D0D0u, 0x1234567u};
    std::vector<uint8_t> a(64 * 1024);
    for (auto& b : a) b = static_cast<uint8_t>(rng.next());
    std::vector<uint8_t> b = a;  // the SAME frame repeated
    const uint32_t ea = static_cast<uint32_t>(a.size());
    const CrcDev::Result ra = t.stream(a, ea);
    const CrcDev::Result rb = t.stream(b, ea);
    check(ra.valid && rb.valid, "repeat: both valid", 1, ra.valid && rb.valid);
    check(ra.crc == rb.crc, "repeat: repeated frame CRCs identical (60 Hz law)", ra.crc, rb.crc);
    b[b.size() / 2] ^= 0x01;  // one flipped byte
    const CrcDev::Result rc = t.stream(b, ea);
    check(rc.crc != ra.crc, "repeat: a changed frame CRCs differently", ra.crc, rc.crc);
  }

  // ---- 4. mis-sized stream, stray pixels, odd expectations ---------------------
  {
    CrcDev t;
    std::vector<uint8_t> canvas(1024, 0x5A);
    // eof 4 bytes early: mis-sized -> error, no published CRC
    CrcDev::Result r = t.stream(std::vector<uint8_t>(canvas.begin(), canvas.end() - 4), 1024);
    check(r.err, "size: mis-sized stream flagged", 1, r.err);
    check(!r.valid, "size: CRC not published", 0, r.valid);
    check(r.bytes_captured == 1020, "size: the length actually captured", 1020, r.bytes_captured);
    zref::Crc32c model;
    const zref::Crc32c::Result m =
        model.frame(std::vector<uint8_t>(canvas.begin(), canvas.end() - 4), 1024);
    check(m.size_err && !m.valid, "size: zref::Crc32c agrees (err, no publish)", 1, m.size_err);

    // an ODD expectation. A pixel-granular stream is always an even number of
    // bytes long, so no stream can ever satisfy it — this is a statement about
    // the expectation, not about the raster, and the device must refuse rather
    // than round to the nearest pixel.
    const CrcDev::Result ro = t.stream(canvas, 1023);
    check(ro.err && !ro.valid, "size: odd expectation can never be met", 1, ro.err);
    const CrcDev::Result ro2 = t.stream(canvas, 1025);
    check(ro2.err && !ro2.valid, "size: odd expectation (above) refused too", 1, ro2.err);

    // the smallest lawful frame: ONE pixel, expecting exactly two bytes
    const std::vector<uint8_t> two = {0x9Cu, 0x3Fu};
    const CrcDev::Result r1 = t.stream(two, 2);
    check(r1.valid && !r1.err, "size: single-pixel frame publishes", 1, r1.valid);
    check(r1.crc == zhao_abi::zhao_crc32c(0, two.data(), 2), "size: single-pixel CRC",
          zhao_abi::zhao_crc32c(0, two.data(), 2), r1.crc);
    check(r1.bytes_captured == 2, "size: single-pixel length", 2, r1.bytes_captured);
    // the same single pixel against any other expectation is mis-sized
    const CrcDev::Result r1b = t.stream(two, 4);
    check(r1b.err && !r1b.valid, "size: single-pixel frame, wrong expectation", 1, r1b.err);

    // a pixel outside any frame (no sof since idle): raster violation
    t.park();
    t.top_->in_valid_i = 1;
    t.top_->in_px_i = 0x4243;
    t.top_->in_sof_i = 0;
    t.top_->in_eof_i = 0;
    t.top_->expect_bytes_i = 2;
    t.top_->eval();
    t.top_->clk = 1;
    t.top_->eval();
    t.top_->clk = 0;
    t.top_->eval();
    check(t.top_->size_err_evt_o != 0, "size: stray pixel flagged", 1, t.top_->size_err_evt_o);
    check(t.top_->bytes_captured_o == 0, "size: a stray pixel captured no frame", 0,
          t.top_->bytes_captured_o);
    check(t.top_->frame_crc_valid_o == 0, "size: stray pixel publishes nothing", 0,
          t.top_->frame_crc_valid_o);
  }

  // ---- 5. byte order: the LOW half of a pixel is the FIRST displayed byte ------
  // This is the one law the move from a byte port to a pixel port could have
  // silently inverted, so it is stated directly rather than left to the random
  // lane. video_rules.md §3: RGB565 little-endian halfwords.
  {
    CrcDev t;
    const std::vector<uint8_t> le = {0x34u, 0x12u, 0x78u, 0x56u};  // pixels 0x1234, 0x5678
    const std::vector<uint8_t> be = {0x12u, 0x34u, 0x56u, 0x78u};  // the swapped reading
    const CrcDev::Result r = t.stream(le, 4);
    check(r.crc == zhao_abi::zhao_crc32c(0, le.data(), 4), "order: low byte first",
          zhao_abi::zhao_crc32c(0, le.data(), 4), r.crc);
    check(r.crc != zhao_abi::zhao_crc32c(0, be.data(), 4),
          "order: NOT the byte-swapped reading (the vectors must differ)", 0, 1);
    // drive the same pixels through the raw pixel port to prove the packing
    // helper is not the thing under test
    const std::vector<uint16_t> px = {0x1234u, 0x5678u};
    const CrcDev::Result rp = t.streamPx(px, 4);
    check(rp.crc == r.crc, "order: raw pixel port agrees with the byte statement", r.crc, rp.crc);
  }

  // ---- 6. expect_bytes_i is LATCHED at sof ------------------------------------
  // The contract says sof "restarts the CRC, latches expect_bytes_i". Nothing
  // in the shell can currently move that value mid-frame (the mode latch is a
  // frame-start law), so without this case the latch is unobservable and a
  // device that simply read the live input would look identical. It is the
  // law, so it gets a test rather than a comment.
  {
    CrcDev t;
    Pcg32 rng{0xBEEF0022u, 0xA5A5A5A5u};
    std::vector<uint8_t> canvas(2048);
    for (auto& b : canvas) b = static_cast<uint8_t>(rng.next());
    const std::vector<uint16_t> px = pack_pixels(canvas);
    const uint32_t nb = static_cast<uint32_t>(canvas.size());
    const uint32_t want = zhao_abi::zhao_crc32c(0, canvas.data(), nb);

    // right at sof, garbage afterwards: publishes, because the latch holds
    const CrcDev::Result rl = t.streamPx(px, nb, 0xFFFFFFFFu, true);
    check(rl.valid && !rl.err, "latch: sof value wins over later input", 1, rl.valid);
    check(rl.crc == want, "latch: and the CRC is still the frame's", want, rl.crc);

    // wrong at sof, right afterwards: refused, for the same reason
    const CrcDev::Result rw = t.streamPx(px, nb + 2, nb, true);
    check(rw.err && !rw.valid, "latch: a later correction does not rescue the frame", 1, rw.err);
  }

  // ---- 7. a sof INSIDE an open frame restarts it -------------------------------
  // The contract says sof "restarts the CRC". A lawful raster never emits two
  // sofs without an eof between them, so this is unreachable in the shell —
  // which is exactly why it needs stating: without it, a device that ignored a
  // second sof and kept accumulating would be indistinguishable. What the law
  // means is that the LATER framing wins: the published CRC covers the tail
  // from the second sof onward and nothing before it.
  {
    CrcDev t;
    Pcg32 rng{0x5EC0ND50u, 0x13579BDFu};
    std::vector<uint8_t> head(64), tail(96);
    for (auto& b : head) b = static_cast<uint8_t>(rng.next());
    for (auto& b : tail) b = static_cast<uint8_t>(rng.next());
    const std::vector<uint16_t> hpx = pack_pixels(head), tpx = pack_pixels(tail);
    const uint32_t want = zhao_abi::zhao_crc32c(0, tail.data(), tail.size());

    // head pixels (sof on the first), then the tail with its own sof and eof
    for (size_t i = 0; i < hpx.size(); ++i) {
      t.top_->in_valid_i = 1;
      t.top_->in_px_i = hpx[i];
      t.top_->in_sof_i = (i == 0) ? 1 : 0;
      t.top_->in_eof_i = 0;
      t.top_->expect_bytes_i = 0xDEADBEEFu;  // a stale expectation the restart drops
      t.top_->eval();
      t.top_->clk = 1;
      t.top_->eval();
      t.top_->clk = 0;
      t.top_->eval();
    }
    const CrcDev::Result r = t.streamPx(tpx, static_cast<uint32_t>(tail.size()));
    check(r.valid && !r.err, "restart: the re-framed frame publishes", 1, r.valid);
    check(r.crc == want, "restart: it covers only the pixels after the second sof", want, r.crc);
    check(r.bytes_captured == tail.size(), "restart: and only their length",
          static_cast<uint32_t>(tail.size()), r.bytes_captured);
  }

  return zhao::report_and_exit("debug_crc_directed");
}
