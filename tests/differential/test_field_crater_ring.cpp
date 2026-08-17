// test_field_crater_ring.cpp — the W5 acceptance test (spec/form/field-ir.md
// §12): compiles the TS-generated wrapper, loads + re-validates, owns the
// golden .zvec (C++ ORACLE), replays, resolves the RING source map, round-
// trips a synthetic .zcap (source ID + program hash), and runs the minimize
// demo with an injected flipped lane (failing-vector discipline, charter
// §20.3 / §29-17).
//
// Artifacts committed as evidence:
//   captures/golden/field/crater_ring.zvec           (generated here, 1st run)
//   captures/failures/field/fail-<hash>-0x5A17.zvec  (minimize demo)
//   captures/failures/field/fail-<hash>-0x5A17.txt   (divergence report)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "crater_ring.hpp"  // TS-generated (compiler/tests/generated)

#include "zfield/zfield.hpp"
#include "zref/zref_frame.hpp"

namespace {

// ---- byte helpers ----------------------------------------------------------

std::vector<uint8_t> readFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return {};
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buf(n);
  if (n > 0 && fread(buf.data(), 1, n, f) != (size_t)n) buf.clear();
  fclose(f);
  return buf;
}

bool writeFile(const std::string& path, const std::vector<uint8_t>& b) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool ok = b.empty() || fwrite(b.data(), 1, b.size(), f) == b.size();
  fclose(f);
  return ok;
}

void put16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(x & 0xFF);
  v.push_back(x >> 8);
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
  put16(v, x & 0xFFFF);
  put16(v, x >> 16);
}
void puti32(std::vector<uint8_t>& v, int32_t x) { put32(v, (uint32_t)x); }

// ---- §6.2 vector generation (pure function of hash+seed+N; mirrors the TS
// generator — vector POLICY, not op semantics) -------------------------------

uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = (uint32_t)(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct VecPRNG {
  uint32_t state;
  explicit VecPRNG(uint32_t program_hash, uint64_t seed) : state((uint32_t)seed ^ program_hash) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
  int32_t lane(int32_t min, int32_t max) {
    const uint32_t r = draw();
    const uint32_t minU = (uint32_t)min;
    const uint32_t count = (uint32_t)((uint32_t)max - minU + 1u);  // 0 = full range
    if (count == 0) return (int32_t)r;
    return (int32_t)(minU + (r % count));
  }
};

struct Bounds {
  int32_t min, max;
};

std::vector<std::vector<int32_t>> generateInputs(uint32_t hash, uint64_t seed, size_t n,
                                                 const std::vector<Bounds>& b) {
  VecPRNG prng(hash, seed);
  std::vector<std::vector<int32_t>> recs;
  const auto clampIn = [](const Bounds& bb, int32_t v) {
    return v < bb.min ? bb.min : (v > bb.max ? bb.max : v);
  };
  recs.push_back({});
  for (auto& bb : b) recs.back().push_back(bb.min);
  recs.push_back({});
  for (auto& bb : b) recs.back().push_back(bb.max);
  recs.push_back({});
  for (auto& bb : b) recs.back().push_back(clampIn(bb, 0));
  for (size_t l = 0; l < b.size(); ++l) {
    recs.push_back({});
    for (size_t i = 0; i < b.size(); ++i) {
      recs.back().push_back(i == l ? b[i].min : prng.lane(b[i].min, b[i].max));
    }
  }
  for (size_t k = 0; k < n; ++k) {
    recs.push_back({});
    for (size_t i = 0; i < b.size(); ++i) {
      recs.back().push_back(prng.lane(b[i].min, b[i].max));
    }
  }
  return recs;
}

// ---- .zvec (field-ir.md §6.1) ----------------------------------------------

constexpr uint32_t ZVEC_MAGIC = 0x5649465Au;

std::vector<uint8_t> encodeZvec(uint32_t hash, uint64_t seed, size_t inN, size_t outN,
                                const std::vector<std::vector<int32_t>>& inputs,
                                const std::vector<std::vector<int32_t>>& expected,
                                const std::vector<uint32_t>& status) {
  std::vector<uint8_t> v;
  put32(v, ZVEC_MAGIC);
  put16(v, 1);   // version
  put16(v, 32);  // lane_bits
  put32(v, hash);
  put32(v, (uint32_t)(seed & 0xFFFFFFFF));
  put32(v, (uint32_t)(seed >> 32));
  put32(v, (uint32_t)inputs.size());
  v.push_back((uint8_t)inN);
  v.push_back((uint8_t)outN);
  put16(v, 0);  // rsvd
  put32(v, 0);  // crc placeholder
  for (size_t i = 0; i < inputs.size(); ++i) {
    for (int32_t x : inputs[i]) puti32(v, x);
    for (int32_t x : expected[i]) puti32(v, x);
    put32(v, status[i]);
  }
  const uint32_t crc = zhao_abi::zhao_crc32c(0, v.data(), v.size());  // field zeroed
  v[28] = crc & 0xFF;
  v[29] = (crc >> 8) & 0xFF;
  v[30] = (crc >> 16) & 0xFF;
  v[31] = (crc >> 24) & 0xFF;
  return v;
}

int failures = 0;
#define CHECK(cond, msg)         \
  do {                           \
    if (!(cond)) {               \
      printf("FAIL: %s\n", msg); \
      ++failures;                \
    } else {                     \
      printf("ok: %s\n", msg);   \
    }                            \
  } while (0)

}  // namespace

int main() {
  using namespace zfield_gen::crater_ring;

  // ---- check 1: load, re-validate, hash -----------------------------------
  const zfield::DecodeResult dec = zfield::decode(kProgramBytes.data(), kProgramBytesLen);
  if (dec.error != zfield::DecodeError::kOk) {
    fprintf(stderr, "DECODE FAILED: %s (%s)\n", zfield::decodeErrorName(dec.error),
            dec.detail.c_str());
  }
  CHECK(dec.error == zfield::DecodeError::kOk, "gate 1: .zprog decodes + fully re-validates");
  CHECK(dec.prog.program_hash == kProgramHash,
        "gate 1: program hash matches the wrapper static_assert");
  CHECK(zfield::programHashOfBytes(kProgramBytes.data(), kProgramBytesLen) == kProgramHash,
        "gate 1: hash recomputes from the embedded bytes");
  printf("crater_ring program hash = 0x%08x (%zu instructions)\n", dec.prog.program_hash,
         dec.prog.instrs.size());

  // negative loads: the loader never trusts bytes (§4)
  {
    std::vector<uint8_t> bad(kProgramBytes.begin(), kProgramBytes.end());
    bad[27] ^= 0xFF;
    CHECK(zfield::decode(bad.data(), bad.size()).error == zfield::DecodeError::kBadCrc,
          "gate 1: corrupted body CRC rejected");
    bad = {kProgramBytes.begin(), kProgramBytes.end()};
    bad[0] = 'X';
    CHECK(zfield::decode(bad.data(), bad.size()).error == zfield::DecodeError::kBadMagic,
          "gate 1: bad magic rejected");
    CHECK(zfield::decode(kProgramBytes.data(), kProgramBytesLen - 1).error ==
              zfield::DecodeError::kBadLength,
          "gate 1: truncated image rejected");
  }

  std::vector<Bounds> bounds;
  for (const zfield::IoLane& l : dec.prog.in_lanes) bounds.push_back({l.min, l.max});
  CHECK(bounds.size() == 12, "earth input record has 12 lanes");

  const uint64_t SEED = 0x5A17;  // FORM §16 echo (field-ir.md §6.2)
  const size_t N = 256;
  const std::vector<std::vector<int32_t>> inputs =
      generateInputs(dec.prog.program_hash, SEED, N, bounds);
  CHECK(inputs.size() == 3 + bounds.size() + N, "corner sequence + N records");

  std::vector<std::vector<int32_t>> expected;
  std::vector<uint32_t> status;
  for (const auto& in : inputs) {
    int32_t out[4] = {0};
    const zfield::Status st = zfield::interpret(dec.prog, in.data(), in.size(), out, 4);
    expected.push_back({out[0], out[1], out[2], out[3]});
    status.push_back((st.sat ? 1u : 0u) | (st.rcp0 ? 2u : 0u));
  }
  // replay again — determinism of the oracle itself
  {
    bool identical = true;
    for (size_t i = 0; i < inputs.size() && identical; ++i) {
      int32_t out[4] = {0};
      zfield::interpret(dec.prog, inputs[i].data(), inputs[i].size(), out, 4);
      for (int k = 0; k < 4; ++k) {
        if (out[k] != expected[i][k]) {
          identical = false;
          break;
        }
      }
    }
    CHECK(identical, "gate 3: second replay is bit-identical");
  }

  const std::string goldenPath = ZHAO_CAPTURE_DIR "/golden/field/crater_ring.zvec";
  const std::vector<uint8_t> zvec =
      encodeZvec(dec.prog.program_hash, SEED, 12, 4, inputs, expected, status);
  if (readFile(goldenPath).empty()) {
    CHECK(writeFile(goldenPath, zvec), "gate 2: golden .zvec WRITTEN (first run) — commit it");
  } else {
    const std::vector<uint8_t> committed = readFile(goldenPath);
    CHECK(committed == zvec, "gate 2: committed golden .zvec is byte-identical");
    CHECK(zhao_abi::zhao_crc32c(0, committed.data() + 32, committed.size() - 32) != 0,
          "gate 2: golden carries a nonzero body");
  }

  const zfield::SourceRef& m = dec.prog.src_map[kRING_Pc];
  CHECK(dec.prog.instrs[kRING_Pc].op == zfield::OP_RING, "gate 4: RING at kRING_Pc");
  CHECK(m.source_id == kRING_Span.source_id && m.line == kRING_Span.line && m.col == kRING_Span.col,
        "gate 4: PC->source map resolves the RING builder span");
  CHECK(dec.prog.source_id == kRING_Span.source_id,
        "gate 4: program source id is the field-program site (kind 3)");

  {
    const std::string zcapPath = ZHAO_CAPTURE_DIR "/golden/field/crater_ring_roundtrip.zcap";
    zhao::ZhaoZcapWriter w(zcapPath);
    zhao::ZhaoSourceMap sm;
    sm.files = {"<module-0>", "spells/upheaval.form"};
    zhao::ZhaoSourceMapEntry e;
    e.source_id = dec.prog.source_id;
    e.module_id = 1;
    e.file_index = 1;
    e.kind = 3;  // field program (capture_format.md §5)
    e.flags = 1;
    e.span_begin = kRING_Span.line;
    e.span_end = kRING_Span.line;
    e.name = "crater_ring";
    e.file = sm.files[1];
    e.program_hash = dec.prog.program_hash;
    sm.entries.push_back(e);
    const zhao::ZhaoSourceMapBuildResult built = zhao::zhao_zcap_build_source_map(sm);
    CHECK(built.ok(), "gate 5: canonical SOURCE_MAP built");
    w.add_section(zhao::ZHAO_ZCAP_SOURCE_MAP, 1, built.bytes);
    std::vector<zhao::ZhaoResourcePage> pages;
    zhao::ZhaoResourcePage pg;
    pg.kind = 3;  // field program page
    pg.page_id = 1;
    pg.byte_length = kProgramBytesLen;
    char ref[64];
    snprintf(ref, sizeof(ref), "field:0x%08x", dec.prog.program_hash);
    pg.ref = ref;
    pages.push_back(pg);
    w.add_section(zhao::ZHAO_ZCAP_RESOURCE_PAGES, 1, zhao::zhao_zcap_build_resource_pages(pages));
    CHECK(w.close(), "gate 5: .zcap written");

    zhao::ZhaoZcapReader r(zcapPath);
    CHECK(r.open() == zhao::ZhaoZcapError::kOk, "gate 5: .zcap reopened");
    const zhao::ZhaoZcapSectionInfo* smSec = r.find(zhao::ZHAO_ZCAP_SOURCE_MAP);
    CHECK(smSec != nullptr, "gate 5: SOURCE_MAP section present");
    std::vector<uint8_t> body;
    bool ok = smSec && r.read_body(*smSec, body);
    const zhao::ZhaoSourceMapParseResult back = zhao::zhao_zcap_parse_source_map(body);
    ok = ok && back.ok() && back.map.entries.size() == 1
         && back.map.entries[0].source_id == dec.prog.source_id
         && back.map.entries[0].name == "crater_ring"
         && back.map.entries[0].program_hash == dec.prog.program_hash;
    CHECK(ok, "gate 5: source ID and program hash survive the round-trip");
    const zhao::ZhaoZcapSectionInfo* pgSec = r.find(zhao::ZHAO_ZCAP_RESOURCE_PAGES);
    std::vector<uint8_t> pbody;
    ok = pgSec && r.read_body(*pgSec, pbody);
    const std::vector<zhao::ZhaoResourcePage> pback =
        zhao::zhao_zcap_parse_resource_pages(pbody.data(), pbody.size());
    char wantRef[64];
    snprintf(wantRef, sizeof(wantRef), "field:0x%08x", dec.prog.program_hash);
    ok = ok && pback.size() == 1 && pback[0].ref == wantRef;
    CHECK(ok, "gate 5: program hash survives the round-trip");
    remove(zcapPath.c_str());  // scratch file, not evidence
  }

  {
    // divergence oracle: expected lane 0 (height) with bit 31 flipped on ONE
    // record; the interpreter "under test" is the real C++ interpreter, the
    // corrupted golden is the injected fault.
    const size_t faultIdx = 7;  // a uniform record
    auto fails = [&](const std::vector<int32_t>& in) {
      int32_t out[4] = {0};
      zfield::interpret(dec.prog, in.data(), in.size(), out, 4);
      return out[0] != (expected[faultIdx][0] ^ 0x80000000);
    };
    std::vector<int32_t> cur = inputs[faultIdx];
    CHECK(fails(cur), "gate 6: injected flipped lane diverges");
    // §6.3 minimize: per-lane bisection toward the nearer bound, <= 64 steps
    for (size_t i = 0; i < cur.size(); ++i) {
      int steps = 0;
      while (steps < 64) {
        const int32_t v = cur[i];
        const int32_t target = (int64_t)v - bounds[i].min <= (int64_t)bounds[i].max - v
                                   ? bounds[i].min
                                   : bounds[i].max;
        if (v == target) break;
        const int32_t mid = target > v ? (int32_t)(((int64_t)v + target + 1) / 2)
                                       : (int32_t)(((int64_t)v + target - 1) / 2);
        if (mid == v) break;
        std::vector<int32_t> next = cur;
        next[i] = mid;
        if (fails(next))
          cur = next;
        else
          break;
        ++steps;
      }
    }
    CHECK(fails(cur), "gate 6: minimized record still fails (kept the failure)");

    // expected-vs-actual report on the minimized record
    int32_t out[4] = {0};
    const zfield::Status actSt = zfield::interpret(dec.prog, cur.data(), cur.size(), out, 4);
    const uint32_t actWord = (actSt.sat ? 1u : 0u) | (actSt.rcp0 ? 2u : 0u);
    const uint32_t stWord = status[faultIdx];
    char report[512];
    snprintf(report, sizeof(report),
             "vector_index=%zu first_divergent_lane=0 expected=0x%08x "
             "actual=0x%08x status_diff=0x%x\n",
             faultIdx, (uint32_t)(expected[faultIdx][0] ^ 0x80000000), (uint32_t)out[0],
             stWord ^ actWord);
    const std::string failBase = ZHAO_CAPTURE_DIR "/failures/field/fail-" + [](uint32_t h) {
      char b[32];
      snprintf(b, sizeof(b), "%08x", h);
      return std::string(b);
    }(dec.prog.program_hash) + "-0x5A17";
    const std::vector<uint8_t> failVec =
        encodeZvec(dec.prog.program_hash, SEED, 12, 4, {cur},
                   {{(int32_t)((uint32_t)expected[faultIdx][0] ^ 0x80000000u),
                     expected[faultIdx][1], expected[faultIdx][2], expected[faultIdx][3]}},
                   {stWord});
    if (readFile(failBase + ".zvec").empty()) {
      CHECK(writeFile(failBase + ".zvec", failVec) &&
                writeFile(failBase + ".txt", std::vector<uint8_t>(report, report + strlen(report))),
            "gate 6: failing vector + report WRITTEN (first run) — commit them");
    } else {
      CHECK(readFile(failBase + ".zvec") == failVec,
            "gate 6: committed failing vector replays (minimized record stable)");
      const std::vector<uint8_t> rep = readFile(failBase + ".txt");
      CHECK(rep == std::vector<uint8_t>(report, report + strlen(report)),
            "gate 6: committed divergence report stable");
    }
    printf("minimized failing input lanes:");
    for (int32_t v : cur) printf(" %d", v);
    printf("\n%s", report);
  }

  {
    CraterRingIn in{};
    in.x = inputs[20][0];
    in.z = inputs[20][1];
    in.age = inputs[20][2];
    in.phase = inputs[20][3];
    for (int i = 0; i < 8; ++i) in.p[i] = inputs[20][4 + i];
    CraterRingOut out{};
    const zfield::Status st = eval(in, out);
    CHECK(out.height == expected[20][0] && out.velocity == expected[20][1] &&
              out.material == expected[20][2] && out.nav_cost == expected[20][3],
          "wrapper: typed eval matches the generic interpreter");
    (void)st;
  }

  // ---- review C1 (RUN-20260814-1912): SPLINE hand-computed unit vectors --
  // Uniform 5-knot table x = 0..4 step 0x10000, y = 0,0,1,1,1;
  // dy_i = round_half_up((1<<32)/Δx) = 0x10000 (§3.15 spline kind).
  // Mirrored unit test: compiler/tests/field_ir.test.ts (same derivation).
  {
    zfield::Decoded p;
    p.tables.push_back(zfield::Table{1,
                                     {0, 1 << 16, 2 << 16, 3 << 16, 4 << 16},
                                     {0, 0, 1 << 16, 1 << 16, 1 << 16},
                                     {1 << 16, 1 << 16, 1 << 16, 1 << 16, 1 << 16}});
    p.in_lanes.push_back(zfield::IoLane{"a", 0, 0, 0, 5 << 16});
    p.out_lanes.push_back(zfield::IoLane{"s", 0, 1, 0, 0});
    p.instrs.push_back(zfield::Instr{zfield::OP_SPLINE, 1, 0, 0, 0, 0});
    p.instrs.push_back(zfield::Instr{zfield::OP_END, 0, 0, 0, 0, 0});

    // Hand derivation, segment i=1, t=0.5 (a = 1.5 = 0x18000):
    //   t  = rescale((0x18000−0x10000)·0x10000, 16) = 0x8000
    //   P0=0, P1=0, P2=0x10000, P3=0x10000
    //   C1 = 0x10000;  C2 = 3·0x10000;  C3 = −2·0x10000
    //   u  = fx_mad(0x8000, −0x20000, 0x30000) = rescale(2^33, 16) = 0x20000
    //   u  = fx_mad(0x8000,  0x20000,  0x10000) = rescale(2^33, 16) = 0x20000
    //   v  = fx_mul(0x8000, 0x20000) = rescale(2^32, 16) = 0x10000
    //   dst = fx_add(0, rescale(0x10000, 1)) = (0x10000+1)>>1 = 0x8000
    // CR ground truth (−P0+9P1+9P2−P3)/16 = 8/16 = 0.5 → 32768. The pre-fix
    // law `rescale(v<<16, 1)` saturated this input to 0x7FFFFFFF.
    // Segment i=2, t=0.75 (a = 2.75 = 0x2C000):
    //   C1 = 0x10000;  C2 = −2·0x10000;  C3 = 0x10000;  t = 0xC000
    //   u  = fx_mad(0xC000, 0x10000, −0x20000) = rescale(−5·2^30, 16) = −0x14000
    //   u  = fx_mad(0xC000, −0x14000, 0x10000) = rescale(2^28, 16) = 0x1000
    //   v  = fx_mul(0xC000, 0x1000) = rescale(3·2^26, 16) = 0xC00
    //   dst = fx_add(0x10000, rescale(0xC00, 1)) = 0x10000 + 0x600 = 0x10600
    // (CR tail overshoot 1.0234375; the pre-fix law gave 0x10000+0x60000000.)
    const struct {
      int32_t a;
      int32_t want;
      const char* why;
    } vecs[] = {
        {0, 0, "spline knot t=0 (i=0): y[0] exact"},
        {1 << 16, 0, "spline knot t=0 (i=1): y[1] exact"},
        {0x18000, 0x8000, "spline midpoint t=0.5 (i=1): 32768"},
        {0x2C000, 0x10600, "spline near-endpoint t=0.75 (i=2): 67072"},
        {5 << 16, 1 << 16, "spline beyond domain clamps to the last knot"},
    };
    for (const auto& v : vecs) {
      int32_t out[1] = {0};
      const zfield::Status st = zfield::interpret(p, &v.a, 1, out, 1);
      char msg[128];
      snprintf(msg, sizeof(msg), "%s (a=0x%x)", v.why, (unsigned)v.a);
      CHECK(out[0] == v.want, msg);
      CHECK(!st.sat, "spline hand vector saturates nothing");
    }
  }

  if (failures != 0) {
    printf("%d FAILURE(S)\n", failures);
    return 1;
  }
  printf("field crater_ring: all gates green\n");
  return 0;
}
