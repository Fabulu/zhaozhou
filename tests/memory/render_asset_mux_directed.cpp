// render_asset_mux_directed.cpp -- Packet-E3 excluded ENGINE1 mux gate.
//
// Production mode covers held whole-request arbitration, real MEM.GUARD
// acceptance-versus-verdict timing, exact 16/32/64-byte routing, malformed
// requests, all reachable protocol detectors, quiet/clear, counters, and the
// reset-only geometry structural barrier. Six compile-time inverse modes each
// require one exact committed-selector signature and no unrelated state.
#if (defined(EXPECT_RENDER_ASSET_HOLD_GUARD_VALID_MUTANT) + \
     defined(EXPECT_RENDER_ASSET_DRIFT_SUBOWNER_MUTANT) +   \
     defined(EXPECT_RENDER_ASSET_TEXTURE_PACK64_MUTANT) +   \
     defined(EXPECT_RENDER_ASSET_TERMINATE_BEAT7_MUTANT) +  \
     defined(EXPECT_RENDER_ASSET_ACCEPT_BEAT9_MUTANT) +     \
     defined(EXPECT_RENDER_ASSET_DENIAL_SILENCE_MUTANT)) > 1
#error "RENDER_ASSET_MUX_DRIVER_SELECTOR_COLLISION: define exactly one inverse branch"
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "verilated.h"
#include "Vtb_render_asset_mux.h"

double sc_time_stamp() { return 0.0; }

namespace {
using Dut = Vtb_render_asset_mux;
constexpr uint32_t kAssetBase = 0x06A00000u;
constexpr uint32_t kAssetEnd = 0x08000000u;
constexpr uint8_t kEngine1 = 3;
constexpr uint8_t kClient5 = 5;
constexpr uint8_t kNone = 7;

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* what, uint64_t expected = 1, uint64_t actual = 0) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("FAIL: %s (expected %llu, got %llu)\n", what,
                static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
  }
}

[[noreturn]] void report_and_exit(const char* suite) {
  if (g_failures != 0) {
    std::printf("[%s] %d of %d checks FAILED\n", suite, g_failures, g_checks);
    std::fflush(nullptr);
    std::_Exit(1);
  }
  std::printf("[%s] %d checks passed\n", suite, g_checks);
  std::fflush(nullptr);
  std::_Exit(0);
}

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  d.clk = 0;
  d.eval();
}

uint64_t be_for_len(unsigned len) {
  if (len >= 64) return UINT64_MAX;
  return (UINT64_C(1) << len) - UINT64_C(1);
}

void drive_defaults(Dut& d) {
  d.frame_fault_clear = 0;
  d.geom_valid = 0;
  d.geom_write = 0;
  d.geom_client = kEngine1;
  d.geom_addr = kAssetBase;
  d.geom_len = 32;
  d.geom_be = be_for_len(32);
  d.texture_valid = 0;
  d.texture_addr = kAssetBase;
  d.raw_valid = 0;
  d.raw_data = 0;
  d.raw_last = 0;
  d.arb_grant = 1;
  d.inject_guard_ok = 0;
  d.inject_guard_violation = 0;
}

void reset(Dut& d) {
  d.clk = 0;
  d.rst_n = 0;
  drive_defaults(d);
  d.eval();
  for (int cycle = 0; cycle < 4; ++cycle) tick(d);
  d.rst_n = 1;
  tick(d);
  d.eval();
}

struct ReqSeen {
  bool accepted = false;
  uint8_t client = 0;
  bool write = false;
  uint32_t addr = 0;
  uint8_t len = 0;
  uint64_t be = 0;
};

ReqSeen accept_texture(Dut& d, uint32_t addr, int limit = 40) {
  d.texture_addr = addr;
  d.texture_valid = 1;
  ReqSeen seen;
  for (int cycle = 0; cycle < limit; ++cycle) {
    d.eval();
    if (d.texture_ready) {
      seen.accepted = true;
      seen.client = static_cast<uint8_t>(d.guard_client);
      seen.write = d.guard_write != 0;
      seen.addr = d.guard_addr;
      seen.len = static_cast<uint8_t>(d.guard_len);
      seen.be = d.guard_be;
      tick(d);  // this edge is both local-ready and real-guard acceptance
      d.texture_valid = 0;
      d.eval();
      return seen;
    }
    check(d.geom_ready == 0, "texture offer never raises the unselected geometry ready", 0,
          d.geom_ready);
    tick(d);
  }
  d.texture_valid = 0;
  check(false, "texture offer accepted finitely");
  return seen;
}

ReqSeen accept_geometry(Dut& d, uint32_t addr, uint8_t len, uint64_t be, bool write = false,
                        uint8_t client = kEngine1, int limit = 40) {
  d.geom_addr = addr;
  d.geom_len = len;
  d.geom_be = be;
  d.geom_write = write;
  d.geom_client = client;
  d.geom_valid = 1;
  ReqSeen seen;
  for (int cycle = 0; cycle < limit; ++cycle) {
    d.eval();
    if (d.geom_ready) {
      seen.accepted = true;
      seen.client = static_cast<uint8_t>(d.guard_client);
      seen.write = d.guard_write != 0;
      seen.addr = d.guard_addr;
      seen.len = static_cast<uint8_t>(d.guard_len);
      seen.be = d.guard_be;
      tick(d);
      d.geom_valid = 0;
      d.eval();
      return seen;
    }
    check(d.texture_ready == 0, "geometry offer never raises the unselected texture ready", 0,
          d.texture_ready);
    tick(d);
  }
  d.geom_valid = 0;
  check(false, "geometry offer accepted finitely");
  return seen;
}

void consume_ok_verdict(Dut& d, bool geometry) {
  d.eval();
  check(d.real_guard_ok == 1 && d.real_guard_violation == 0,
        "real guard produces exactly one OK verdict", 1,
        (d.real_guard_ok << 1) | d.real_guard_violation);
  if (geometry) {
    check(d.geom_ok == 1 && d.geom_violation == 0, "geometry receives its typed OK verdict");
  } else {
    check(d.texture_refused == 0, "approved texture request receives no refusal", 0,
          d.texture_refused);
  }
  check(d.guard_valid == 0, "guard valid drops in WAIT_VERDICT", 0, d.guard_valid);
  tick(d);  // mux enters DATA; real guard's held arb request is granted
  d.eval();
}

void consume_deny_verdict(Dut& d, bool geometry) {
  d.eval();
  check(d.real_guard_violation == 1 && d.real_guard_ok == 0,
        "real guard produces exactly one denial verdict", 1,
        (d.real_guard_violation << 1) | d.real_guard_ok);
  check(d.real_guard_ready == 1, "guard ready remains high during a denial verdict");
  check(d.guard_valid == 0, "mux drops guard valid before denial despite ready staying high", 0,
        d.guard_valid);
  if (geometry) {
    check(d.geom_violation == 1 && d.geom_ok == 0, "denied geometry receives one typed violation");
  } else {
    check(d.texture_refused == 1, "denied texture receives one refusal pulse");
  }
  tick(d);  // consume verdict and advance the guard's delayed violation counter
  d.eval();
}

struct RawSeen {
  bool texture_valid = false;
  uint16_t texture_data = 0;
  bool texture_refused = false;
  bool geom_valid = false;
  uint64_t geom_data = 0;
  bool geom_last = false;
};

RawSeen send_raw(Dut& d, uint16_t word, bool last) {
  d.raw_valid = 1;
  d.raw_data = word;
  d.raw_last = last ? 1 : 0;
  d.eval();
  RawSeen seen;
  seen.texture_valid = d.texture_data_valid != 0;
  seen.texture_data = static_cast<uint16_t>(d.texture_data);
  seen.texture_refused = d.texture_refused != 0;
  seen.geom_valid = d.geom_beat_valid != 0;
  seen.geom_data = d.geom_beat_data;
  seen.geom_last = d.geom_beat_last != 0;
  tick(d);
  d.raw_valid = 0;
  d.raw_last = 0;
  d.eval();
  return seen;
}

uint64_t packed4(uint16_t a, uint16_t b, uint16_t c, uint16_t e) {
  return static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 16) |
         (static_cast<uint64_t>(c) << 32) | (static_cast<uint64_t>(e) << 48);
}

void quiet_clear(Dut& d) {
  d.eval();
  check(d.quiet == 1, "fault clear is attempted only at exact quiet", 1, d.quiet);
  const uint32_t ga = d.guard_accepted;
  const uint32_t raw = d.raw_halfwords;
  d.frame_fault_clear = 1;
  tick(d);
  d.frame_fault_clear = 0;
  d.eval();
  check(d.protocol_fault == 0, "quiet frame clear removes recoverable fault", 0, d.protocol_fault);
  check(d.guard_accepted == ga && d.raw_halfwords == raw,
        "frame clear changes no transaction counters", ga, d.guard_accepted);
}

void begin_texture_data(Dut& d, uint32_t addr = kAssetBase) {
  const ReqSeen r = accept_texture(d, addr);
  check(r.accepted, "texture request crosses local/guard acceptance");
  consume_ok_verdict(d, false);
}

void begin_geometry_data(Dut& d, uint32_t addr, uint8_t len) {
  const ReqSeen r = accept_geometry(d, addr, len, be_for_len(len));
  check(r.accepted, "geometry request crosses local/guard acceptance");
  consume_ok_verdict(d, true);
}

void finish_texture_exact(Dut& d, uint16_t base_word,
                          std::vector<RawSeen>* observations = nullptr) {
  for (int beat = 0; beat < 8; ++beat) {
    RawSeen s = send_raw(d, static_cast<uint16_t>(base_word + beat), beat == 7);
    if (observations != nullptr) observations->push_back(s);
  }
}

void finish_geometry_exact(Dut& d, int halfwords, uint16_t base_word,
                           std::vector<RawSeen>* observations = nullptr) {
  for (int beat = 0; beat < halfwords; ++beat) {
    RawSeen s = send_raw(d, static_cast<uint16_t>(base_word + beat), beat == halfwords - 1);
    if (observations != nullptr) observations->push_back(s);
  }
}

void test_legal_16_32_64_and_counters() {
  Dut d;
  reset(d);
  check(d.quiet == 1, "reset reaches exact quiet");
  check(d.protocol_fault == 0 && d.structural_fault == 0, "both fault classes reset clear");

  const ReqSeen tr = accept_texture(d, kAssetBase);
  check(tr.client == kEngine1 && !tr.write && tr.addr == kAssetBase && tr.len == 16 &&
            tr.be == UINT64_C(0xFFFF),
        "texture translation is exact ENGINE1/read/addr/len16/low16-BE", 1, tr.client);
  consume_ok_verdict(d, false);
  std::vector<RawSeen> tex;
  finish_texture_exact(d, 0x1100, &tex);
  bool direct = tex.size() == 8;
  for (int i = 0; i < 8 && direct; ++i) {
    direct = tex[i].texture_valid && tex[i].texture_data == 0x1100 + i && !tex[i].texture_refused &&
             !tex[i].geom_valid;
  }
  check(direct, "texture receives eight raw halfwords directly, never packed");
  check(d.quiet == 1, "legal texture return releases owner at exact eighth LAST");

  const uint32_t g32addr = kAssetBase + 0x120u;
  const ReqSeen g32 = accept_geometry(d, g32addr, 32, be_for_len(32));
  check(g32.client == kEngine1 && !g32.write && g32.addr == g32addr && g32.len == 32 &&
            g32.be == be_for_len(32),
        "geometry-32 preserves addr/len/BE while forcing read ENGINE1");
  consume_ok_verdict(d, true);
  std::vector<RawSeen> geom32;
  finish_geometry_exact(d, 16, 0x2100, &geom32);
  int g32beats = 0;
  bool g32pack = true;
  for (int i = 0; i < 16; ++i) {
    if (geom32[i].geom_valid) {
      const int group = i / 4;
      ++g32beats;
      g32pack = g32pack && ((i & 3) == 3) &&
                geom32[i].geom_data == packed4(static_cast<uint16_t>(0x2100 + group * 4 + 0),
                                               static_cast<uint16_t>(0x2100 + group * 4 + 1),
                                               static_cast<uint16_t>(0x2100 + group * 4 + 2),
                                               static_cast<uint16_t>(0x2100 + group * 4 + 3)) &&
                (geom32[i].geom_last == (group == 3));
    }
    g32pack = g32pack && !geom32[i].texture_valid;
  }
  check(g32beats == 4 && g32pack,
        "32-byte geometry packs four little-endian 64-bit beats, LAST on four", 4, g32beats);

  const uint32_t g64addr = kAssetBase + 0x200u;
  accept_geometry(d, g64addr, 64, be_for_len(64));
  consume_ok_verdict(d, true);
  std::vector<RawSeen> geom64;
  finish_geometry_exact(d, 32, 0x3100, &geom64);
  int g64beats = 0;
  int g64last_at = -1;
  bool g64pack = true;
  for (int i = 0; i < 32; ++i) {
    if (geom64[i].geom_valid) {
      ++g64beats;
      const int group = i / 4;
      g64pack =
          g64pack && geom64[i].geom_data == packed4(static_cast<uint16_t>(0x3100 + group * 4 + 0),
                                                    static_cast<uint16_t>(0x3100 + group * 4 + 1),
                                                    static_cast<uint16_t>(0x3100 + group * 4 + 2),
                                                    static_cast<uint16_t>(0x3100 + group * 4 + 3));
      if (geom64[i].geom_last) g64last_at = g64beats;
    }
  }
  check(g64beats == 8 && g64last_at == 8 && g64pack,
        "64-byte geometry packs eight ordered beats, LAST only on eight", 8, g64beats);
  check(d.guard_accepted == 3 && d.guard_ok_count == 3 && d.guard_denied == 0 &&
            d.geometry_accepted == 2 && d.texture_accepted == 1 && d.geometry_refused == 0 &&
            d.texture_refused_count == 0 && d.raw_halfwords == 56 && d.protocol_faults == 0,
        "legal drain has exact GA/GOK/local partition/raw/fault counters", 56, d.raw_halfwords);
  check(d.guard_accepted == d.guard_ok_count + d.guard_denied &&
            d.guard_accepted == d.geometry_accepted + d.texture_accepted,
        "legal drain identities GA=GOK+GDENY and local accepted partition hold");
  check(d.quiet == 1 && d.protocol_fault == 0 && d.structural_fault == 0,
        "all legal 16/32/64 traffic returns to clean quiet");
}

void test_round_robin_both_histories_and_sustained_contention() {
  {
    Dut d;
    reset(d);
    d.geom_valid = 1;
    d.geom_addr = kAssetBase + 0x400u;
    d.geom_len = 32;
    d.geom_be = be_for_len(32);
    d.texture_valid = 1;
    d.texture_addr = kAssetBase + 0x500u;
    tick(d);  // capture from both; reset history names geometry as last
    d.eval();
    check(d.texture_ready == 1 && d.geom_ready == 0,
          "reset RR history chooses texture and readies only that winner");
    tick(d);
    d.texture_valid = 0;  // geometry remains continuously offered
    d.eval();
    check(d.real_guard_ok == 1, "first contended texture gets guard OK");
    tick(d);
    finish_texture_exact(d, 0x4100);
    check(d.geom_valid == 1 && d.geometry_accepted == 0,
          "losing geometry offer remains held, not dropped");
    tick(d);  // capture held geometry
    d.eval();
    check(d.geom_ready == 1 && d.texture_ready == 0,
          "held loser alone becomes the next whole-request winner");
    tick(d);
    d.geom_valid = 0;
    d.eval();
    tick(d);
    finish_geometry_exact(d, 16, 0x4200);
    check(d.texture_accepted == 1 && d.geometry_accepted == 1 && d.contention == 1 &&
              d.protocol_faults == 0,
          "sustained contention serves both once and counts one arbitration", 1, d.contention);
  }

  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetBase + 0x600u);  // rr history now TEXTURE
    finish_texture_exact(d, 0x4300);
    d.geom_valid = 1;
    d.geom_addr = kAssetBase + 0x700u;
    d.geom_len = 32;
    d.geom_be = be_for_len(32);
    d.texture_valid = 1;
    d.texture_addr = kAssetBase + 0x800u;
    tick(d);
    d.eval();
    check(d.geom_ready == 1 && d.texture_ready == 0,
          "texture-last RR history chooses geometry next");
    tick(d);
    d.geom_valid = 0;
    d.eval();
    tick(d);
    finish_geometry_exact(d, 16, 0x4400);
    tick(d);  // capture still-held texture loser
    d.eval();
    check(d.texture_ready == 1 && d.geom_ready == 0,
          "opposite-history loser is also retained and admitted");
    tick(d);
    d.texture_valid = 0;
    d.eval();
    tick(d);
    finish_texture_exact(d, 0x4500);
    check(d.geometry_accepted == 1 && d.texture_accepted == 2 && d.contention == 1 &&
              d.protocol_faults == 0,
          "opposite RR history drains without starvation or fault");
  }
}

void expect_texture_denial(Dut& d, uint32_t addr, uint8_t expected_client,
                           const char* translation_check) {
  const ReqSeen r = accept_texture(d, addr);
  check(r.client == expected_client && !r.write && r.len == 16 && r.be == UINT64_C(0xFFFF),
        translation_check, expected_client, r.client);
  consume_deny_verdict(d, false);
  check(d.guard_accepted == 1 && d.guard_denied == 1 && d.guard_ok_count == 0 &&
            d.texture_accepted == 1 && d.texture_refused_count == 1 && d.guard_violations == 1 &&
            d.protocol_faults == 0 && d.quiet == 1,
        "texture denial has exact accept/deny/refusal/guard evidence");
}

void expect_geometry_denial(Dut& d, uint32_t addr, uint8_t len, uint64_t be, bool write,
                            uint8_t client, uint8_t expected_guard_client,
                            const char* translation_check) {
  const ReqSeen r = accept_geometry(d, addr, len, be, write, client);
  check(
      r.client == expected_guard_client && !r.write && r.addr == addr && r.len == len && r.be == be,
      translation_check, expected_guard_client, r.client);
  consume_deny_verdict(d, true);
  check(d.guard_accepted == 1 && d.guard_denied == 1 && d.geometry_accepted == 1 &&
            d.geometry_refused == 1 && d.texture_refused_count == 0 && d.guard_violations == 1 &&
            d.protocol_faults == 0 && d.quiet == 1,
        "geometry denial has exact typed refusal and no texture side effect");
}

void test_boundaries_and_malformed_translation() {
  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetEnd - 16u);
    finish_texture_exact(d, 0x5100);
    check(d.guard_ok_count == 1 && d.protocol_faults == 0,
          "last aligned 16-byte texture line is accepted at half-open boundary");
  }
  {
    Dut d;
    reset(d);
    begin_geometry_data(d, kAssetEnd - 32u, 32);
    finish_geometry_exact(d, 16, 0x5200);
    check(d.guard_ok_count == 1, "last 32-byte geometry record is accepted at half-open boundary");
  }
  {
    Dut d;
    reset(d);
    // Geometry has no extra alignment law here: addr and BE are preserved and
    // the real guard alone decides containment/full-mask shape.
    begin_geometry_data(d, kAssetBase + 2u, 32);
    finish_geometry_exact(d, 16, 0x5300);
    check(d.guard_ok_count == 1 && d.protocol_faults == 0,
          "misaligned geometry address is preserved rather than silently rounded");
  }
  {
    Dut d;
    reset(d);
    expect_texture_denial(d, kAssetBase + 2u, kNone,
                          "misaligned texture maps to existing CLIENT_NONE, never aligned down");
    check(d.guard_violation_addr == (kAssetBase + 2u),
          "misaligned denial trace retains the offered low 27-bit address", kAssetBase + 2u,
          d.guard_violation_addr);
  }
  {
    Dut d;
    reset(d);
    expect_texture_denial(
        d, kAssetBase - 16u, kEngine1,
        "aligned in-map-width texture below pool keeps ENGINE1 for real range denial");
  }
  {
    Dut d;
    reset(d);
    expect_texture_denial(d, 0x86A00000u, kNone,
                          "upper texture address bits force CLIENT_NONE before 27-bit truncation");
    check(d.guard_violation_addr == kAssetBase,
          "upper-address case proves low bits alias the legal base but remain denied", kAssetBase,
          d.guard_violation_addr);
  }
  {
    Dut d;
    reset(d);
    expect_geometry_denial(d, kAssetBase, 16, be_for_len(16), false, kEngine1, kNone,
                           "geometry len16 remains len16 and maps to CLIENT_NONE, never widened");
  }
  {
    Dut d;
    reset(d);
    expect_geometry_denial(d, kAssetBase, 32, be_for_len(32), true, kEngine1, kNone,
                           "geometry write is forced read but CLIENT_NONE preserves denial");
  }
  {
    Dut d;
    reset(d);
    expect_geometry_denial(d, kAssetBase, 32, UINT64_C(0xFFFF), false, kEngine1, kNone,
                           "wrong geometry BE is preserved and maps to CLIENT_NONE");
  }
  {
    Dut d;
    reset(d);
    expect_geometry_denial(d, kAssetBase, 32, be_for_len(32), false, kClient5, kNone,
                           "unspent client5 cannot become ENGINE1 through the local mux");
    check(d.guard_violation_client == kNone,
          "guard trace contains existing NONE, never global client5", kNone,
          d.guard_violation_client);
  }
}

// Leave a real guard forward stage occupied while the mux has already consumed
// an exact return. This deliberately separates the guard-ready stall mechanism
// from raw data generation so source-hold detectors can be fired deterministically.
void block_guard_with_completed_texture(Dut& d) {
  d.arb_grant = 0;
  begin_texture_data(d, kAssetBase + 0x1000u);
  finish_texture_exact(d, 0x6100);
  d.eval();
  check(d.arb_valid == 1 && d.real_guard_ready == 0 && d.quiet == 1,
        "fixture leaves only real guard forward stage occupied");
}

void release_guard_block(Dut& d) {
  d.arb_grant = 1;
  tick(d);
  d.eval();
}

void test_guard_stall_hold_change_disappearance_and_clear() {
  // Payload drift in the first cycle the previously blocked real guard is ready:
  // cancel A at the would-be acceptance edge, then accept only a clean B.
  {
    Dut d;
    reset(d);
    block_guard_with_completed_texture(d);
    const uint32_t a_addr = kAssetBase + 0x1200u;
    const uint32_t b_addr = kAssetBase + 0x1280u;
    const uint32_t ga0 = d.guard_accepted;
    const uint32_t gok0 = d.guard_ok_count;
    const uint32_t gd0 = d.guard_denied;
    const uint32_t geom_a0 = d.geometry_accepted;
    const uint32_t tex_a0 = d.texture_accepted;
    const uint32_t geom_r0 = d.geometry_refused;
    const uint32_t tex_r0 = d.texture_refused_count;
    const uint32_t raw0 = d.raw_halfwords;
    const uint32_t gv0 = d.guard_violations;
    const uint32_t pf0 = d.protocol_faults;

    d.geom_valid = 1;
    d.geom_addr = a_addr;
    d.geom_len = 32;
    d.geom_be = be_for_len(32);
    tick(d);  // capture A behind the occupied real guard
    d.eval();
    check(d.guard_valid == 1 && d.guard_addr == a_addr && d.real_guard_ready == 0 &&
              d.geom_ready == 0 && d.texture_ready == 0,
          "stalled geometry A is held with neither local source readied");
    const uint64_t held_be = d.guard_be;
    for (int cycle = 0; cycle < 3; ++cycle) {
      check(d.guard_valid == 1 && d.guard_addr == a_addr && d.guard_len == 32 &&
                d.guard_be == held_be && d.guard_client == kEngine1,
            "correct guard-stalled source holds every captured A field stable");
      tick(d);
      d.eval();
    }
    check(d.protocol_faults == pf0, "correct source hold does not fire the lifetime detector", pf0,
          d.protocol_faults);

    d.arb_grant = 1;
    tick(d);  // retire the blocker; held A now sees the real guard ready
    d.eval();
    check(d.real_guard_ready == 1 && d.guard_valid == 1 && d.guard_addr == a_addr &&
              d.geom_ready == 1,
          "held geometry A reaches the decisive newly-ready offer cycle");
    d.geom_addr = b_addr;  // drift immediately before A's acceptance edge
    d.eval();
    check(
        d.real_guard_ready == 1 && d.guard_valid == 0 && d.geom_ready == 0 && d.texture_ready == 0,
        "newly-ready same-cycle geometry drift suppresses guard valid and both local readies");
    check(d.geom_ok == 0 && d.geom_violation == 0 && d.texture_data_valid == 0 &&
              d.texture_refused == 0 && d.geom_beat_valid == 0,
          "drifting A creates no verdict, data, beat, or refusal pulse");
    tick(d);           // detect and cancel A; source is still valid at this edge
    d.geom_valid = 0;  // keep it absent before M_IDLE can capture a new offer
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == pf0 + 1,
          "payload drift sets exactly one recoverable protocol fault", pf0 + 1, d.protocol_faults);
    check(d.guard_valid == 0 && d.geom_ready == 0 && d.texture_ready == 0,
          "detecting edge clears A's offer, capture, owner, and ready");
    check(d.guard_accepted == ga0 && d.geometry_accepted == geom_a0 &&
              d.texture_accepted == tex_a0 && d.guard_ok_count == gok0 && d.guard_denied == gd0 &&
              d.geometry_refused == geom_r0 && d.texture_refused_count == tex_r0 &&
              d.raw_halfwords == raw0 && d.guard_violations == gv0,
          "canceled drifting A changes no physical/local acceptance or disposition counter");

    check(d.real_guard_ready == 1 && d.guard_valid == 0 && d.geom_ready == 0 &&
              d.texture_ready == 0 && d.real_guard_ok == 0 && d.real_guard_violation == 0 &&
              d.geom_ok == 0 && d.geom_violation == 0 && d.texture_data_valid == 0 &&
              d.texture_refused == 0 && d.geom_beat_valid == 0,
          "ready guard cannot accept or answer canceled drifting A");
    for (int cycle = 0; cycle < 3; ++cycle) tick(d);
    d.eval();
    check(d.guard_accepted == ga0 && d.geometry_accepted == geom_a0 &&
              d.texture_accepted == tex_a0 && d.protocol_faults == pf0 + 1 &&
              d.guard_ok_count == gok0 && d.guard_denied == gd0,
          "absent canceled A neither reaccepts nor duplicates its one fault");

    const ReqSeen b = accept_geometry(d, b_addr, 32, be_for_len(32));
    check(b.accepted && b.addr == b_addr && b.client == kEngine1 && d.arb_valid == 1 &&
              d.arb_addr == b_addr,
          "distinct clean geometry B alone forwards after canceled A", b_addr, b.addr);
    consume_ok_verdict(d, true);
    std::vector<RawSeen> returned;
    finish_geometry_exact(d, 16, 0x6200, &returned);
    int beats = 0;
    for (const RawSeen& s : returned) beats += s.geom_valid;
    check(beats == 4 && d.guard_accepted == ga0 + 1 && d.geometry_accepted == geom_a0 + 1 &&
              d.texture_accepted == tex_a0 && d.guard_ok_count == gok0 + 1 &&
              d.guard_denied == gd0 && d.geometry_refused == geom_r0 &&
              d.texture_refused_count == tex_r0 && d.raw_halfwords == raw0 + 16 &&
              d.protocol_faults == pf0 + 1,
          "only geometry B is accepted and returns four exact packed beats", 4, beats);
    check(d.quiet == 1 && d.protocol_fault == 1,
          "drift cancellation plus clean B reaches quiet with one sticky fault");
    quiet_clear(d);
  }

  // Source disappearance in the first cycle the previously blocked real guard
  // is ready: cancel A at the would-be acceptance edge, then issue texture B.
  {
    Dut d;
    reset(d);
    block_guard_with_completed_texture(d);
    const uint32_t a_addr = kAssetBase + 0x1400u;
    const uint32_t b_addr = kAssetBase + 0x1480u;
    const uint32_t ga0 = d.guard_accepted;
    const uint32_t gok0 = d.guard_ok_count;
    const uint32_t gd0 = d.guard_denied;
    const uint32_t geom_a0 = d.geometry_accepted;
    const uint32_t tex_a0 = d.texture_accepted;
    const uint32_t geom_r0 = d.geometry_refused;
    const uint32_t tex_r0 = d.texture_refused_count;
    const uint32_t raw0 = d.raw_halfwords;
    const uint32_t gv0 = d.guard_violations;
    const uint32_t pf0 = d.protocol_faults;

    d.texture_addr = a_addr;
    d.texture_valid = 1;
    tick(d);  // capture A behind the occupied real guard
    d.eval();
    check(d.guard_valid == 1 && d.guard_addr == a_addr && d.real_guard_ready == 0 &&
              d.texture_ready == 0 && d.geom_ready == 0,
          "stalled texture A is captured without any local ready");

    d.arb_grant = 1;
    tick(d);  // retire the blocker; held A now sees the real guard ready
    d.eval();
    check(d.real_guard_ready == 1 && d.guard_valid == 1 && d.guard_addr == a_addr &&
              d.texture_ready == 1,
          "held texture A reaches the decisive newly-ready offer cycle");
    d.texture_valid = 0;  // disappear immediately before A's acceptance edge
    d.eval();
    check(
        d.real_guard_ready == 1 && d.guard_valid == 0 && d.texture_ready == 0 && d.geom_ready == 0,
        "newly-ready same-cycle source disappearance suppresses guard valid and both readies");
    check(d.real_guard_ok == 0 && d.real_guard_violation == 0 && d.geom_ok == 0 &&
              d.geom_violation == 0 && d.texture_data_valid == 0 && d.texture_refused == 0 &&
              d.geom_beat_valid == 0,
          "disappearing A produces no verdict, data, beat, or refusal");
    tick(d);  // detect and cancel A
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == pf0 + 1,
          "source disappearance sets exactly one recoverable fault", pf0 + 1, d.protocol_faults);
    check(d.guard_accepted == ga0 && d.geometry_accepted == geom_a0 &&
              d.texture_accepted == tex_a0 && d.guard_ok_count == gok0 && d.guard_denied == gd0 &&
              d.geometry_refused == geom_r0 && d.texture_refused_count == tex_r0 &&
              d.raw_halfwords == raw0 && d.guard_violations == gv0,
          "canceled absent A changes no physical/local acceptance or terminal count");

    check(d.real_guard_ready == 1 && d.guard_valid == 0 && d.texture_ready == 0 &&
              d.geom_ready == 0 && d.real_guard_ok == 0 && d.real_guard_violation == 0 &&
              d.geom_ok == 0 && d.geom_violation == 0 && d.texture_data_valid == 0 &&
              d.texture_refused == 0 && d.geom_beat_valid == 0,
          "ready guard sees no trace of canceled absent A");
    for (int cycle = 0; cycle < 3; ++cycle) tick(d);
    d.eval();
    check(d.guard_accepted == ga0 && d.texture_accepted == tex_a0 &&
              d.geometry_accepted == geom_a0 && d.protocol_faults == pf0 + 1 &&
              d.guard_ok_count == gok0 && d.guard_denied == gd0,
          "keeping A absent proves zero reacceptance and no duplicate fault");

    const ReqSeen b = accept_texture(d, b_addr);
    check(b.accepted && b.addr == b_addr && b.client == kEngine1 && d.arb_valid == 1 &&
              d.arb_addr == b_addr,
          "distinct clean texture B alone forwards after canceled A", b_addr, b.addr);
    consume_ok_verdict(d, false);
    std::vector<RawSeen> returned;
    finish_texture_exact(d, 0x6300, &returned);
    int direct = 0;
    bool exact_data = true;
    for (int beat = 0; beat < 8; ++beat) {
      direct += returned[beat].texture_valid;
      exact_data = exact_data && returned[beat].texture_valid &&
                   returned[beat].texture_data == static_cast<uint16_t>(0x6300 + beat) &&
                   !returned[beat].geom_valid && !returned[beat].texture_refused;
    }
    check(direct == 8 && exact_data && d.guard_accepted == ga0 + 1 &&
              d.texture_accepted == tex_a0 + 1 && d.geometry_accepted == geom_a0 &&
              d.guard_ok_count == gok0 + 1 && d.guard_denied == gd0 &&
              d.texture_refused_count == tex_r0 && d.geometry_refused == geom_r0 &&
              d.raw_halfwords == raw0 + 8 && d.protocol_faults == pf0 + 1,
          "only texture B is accepted and receives eight exact direct words", 8, direct);
    check(d.quiet == 1 && d.protocol_fault == 1,
          "disappearance cancellation plus clean B reaches quiet with one fault");
    quiet_clear(d);
  }
}

void test_verdict_detectors_and_pre_ok_raw() {
  {
    Dut d;
    reset(d);
    d.inject_guard_ok = 1;
    tick(d);
    d.inject_guard_ok = 0;
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == 1 && d.guard_ok_count == 0 &&
              d.guard_accepted == 0,
          "unsolicited verdict fires without inventing accept/OK counts", 1, d.protocol_faults);
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    accept_texture(d, kAssetBase + 0x1600u);
    check(d.real_guard_ok == 1, "both-verdict fixture has real OK");
    d.inject_guard_violation = 1;
    d.eval();
    check(d.texture_refused == 1 && d.guard_valid == 0,
          "both verdict bits take exactly the denial disposition");
    tick(d);
    d.inject_guard_violation = 0;
    d.eval();
    check(d.guard_denied == 1 && d.guard_ok_count == 0 && d.texture_refused_count == 1 &&
              d.protocol_faults == 1 && d.guard_accepted == 1 && d.structural_fault == 0,
          "both-verdict event counts one denial plus one protocol fault");
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetBase + 0x1800u);
    d.inject_guard_ok = 1;
    tick(d);  // duplicate verdict while already waiting for data
    d.inject_guard_ok = 0;
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == 1 && d.guard_ok_count == 1 &&
              d.guard_accepted == 1,
          "duplicate verdict fires but cannot create a second disposition", 1, d.protocol_faults);
    finish_texture_exact(d, 0x6400);
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    accept_texture(d, kAssetBase + 0x1A00u);
    check(d.real_guard_ok == 1, "pre-OK raw fixture reaches verdict cycle");
    const RawSeen pre = send_raw(d, 0xCAFEu, false);
    check(!pre.texture_valid && !pre.geom_valid,
          "raw before OK is suppressed from both local routes");
    check(d.protocol_fault == 1 && d.protocol_faults == 1 && d.raw_halfwords == 1,
          "raw-before-OK detector and physical raw counter both fire", 1, d.protocol_faults);
    finish_texture_exact(d, 0x6500);
    check(d.texture_refused_count == 0 && d.raw_halfwords == 9,
          "pre-OK raw cannot consume one of the eight approved words", 9, d.raw_halfwords);
    quiet_clear(d);
  }
}

void test_texture_return_faults_and_drain() {
  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetBase + 0x1C00u);
    int delivered = 0;
    for (int beat = 0; beat < 3; ++beat)
      delivered += send_raw(d, static_cast<uint16_t>(0x7100 + beat), false).texture_valid;
    const RawSeen early = send_raw(d, 0x7103u, true);
    check(!early.texture_valid && early.texture_refused && !early.geom_valid,
          "early texture LAST suppresses that word and emits one refusal");
    check(delivered == 3 && d.texture_refused_count == 1 && d.protocol_faults == 1 &&
              d.raw_halfwords == 4 && d.quiet == 1 && d.structural_fault == 0,
          "short texture return terminates recoverably with exact evidence", 3, delivered);
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetBase + 0x1E00u);
    int delivered = 0;
    for (int beat = 0; beat < 7; ++beat)
      delivered += send_raw(d, static_cast<uint16_t>(0x7200 + beat), false).texture_valid;
    const RawSeen eighth = send_raw(d, 0x7207u, false);
    check(!eighth.texture_valid && eighth.texture_refused,
          "eighth texture word without LAST is suppressed and refused");
    check(d.protocol_faults == 1 && d.texture_refused_count == 1 && d.quiet == 0,
          "missing-LAST enters non-quiet drain with one initial fault");

    d.geom_valid = 1;
    d.geom_addr = kAssetBase + 0x2000u;
    d.geom_len = 32;
    d.geom_be = be_for_len(32);
    const RawSeen ninth = send_raw(d, 0x7208u, false);
    check(!ninth.texture_valid && !ninth.geom_valid && !ninth.texture_refused,
          "ninth/surplus word never reaches cache or queued geometry");
    check(d.protocol_faults == 2 && d.geom_ready == 0 && d.guard_valid == 0,
          "ninth word has its own positive control and no new request starts", 2,
          d.protocol_faults);
    const RawSeen surplus_last = send_raw(d, 0x7209u, true);
    check(!surplus_last.texture_valid && !surplus_last.geom_valid,
          "surplus physical LAST is discarded while ending drain");
    check(d.protocol_faults == 3 && d.raw_halfwords == 10,
          "every surplus raw pulse is counted and classified", 10, d.raw_halfwords);
    check(delivered == 7, "only the first seven nonterminal texture words escaped", 7, delivered);
    check(d.geom_ready == 0, "queued geometry is not accepted on the drain-retirement edge", 0,
          d.geom_ready);
    d.geom_valid = 0;
  }
  {
    Dut d;
    reset(d);
    begin_texture_data(d, kAssetBase + 0x2200u);
    d.raw_last = 1;
    tick(d);
    d.raw_last = 0;
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == 1 && d.raw_halfwords == 0,
          "LAST without VALID independently faults and consumes no word", 1, d.protocol_faults);
    finish_texture_exact(d, 0x7300);
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    accept_texture(d, kAssetBase + 2u);
    consume_deny_verdict(d, false);
    const RawSeen late = send_raw(d, 0xDEADu, true);
    check(!late.texture_valid && !late.geom_valid && !late.texture_refused,
          "raw after denial/no owner is suppressed from every output");
    check(d.protocol_faults == 1 && d.raw_halfwords == 1,
          "raw-after-denial/no-owner detector fires exactly once", 1, d.protocol_faults);
    quiet_clear(d);
  }
}

void test_clear_set_priority_and_structural_geometry() {
  {
    Dut d;
    reset(d);
    d.frame_fault_clear = 1;
    d.raw_last = 1;
    tick(d);
    d.frame_fault_clear = 0;
    d.raw_last = 0;
    d.eval();
    check(d.protocol_fault == 1 && d.protocol_faults == 1,
          "malformed LAST-without-VALID set wins same-edge frame clear", 1, d.protocol_faults);
    quiet_clear(d);
  }
  {
    Dut d;
    reset(d);
    begin_geometry_data(d, kAssetBase + 0x2400u, 32);
    int geom_beats = 0;
    for (int beat = 0; beat < 4; ++beat)
      geom_beats += send_raw(d, static_cast<uint16_t>(0x8100 + beat), false).geom_valid;
    const RawSeen early = send_raw(d, 0x8104u, true);
    check(!early.geom_valid && !early.texture_valid && !early.texture_refused,
          "early geometry LAST invents neither an incomplete beat nor refusal");
    check(geom_beats == 1 && d.structural_fault == 1 && d.protocol_fault == 1 &&
              d.protocol_faults == 1 && d.quiet == 0,
          "approved short geometry latches distinct reset-only structural fault", 1,
          d.structural_fault);
    d.texture_valid = 1;
    d.texture_addr = kAssetBase + 0x2600u;
    d.frame_fault_clear = 1;
    for (int cycle = 0; cycle < 3; ++cycle) tick(d);
    d.texture_valid = 0;
    d.frame_fault_clear = 0;
    d.eval();
    check(d.texture_ready == 0 && d.guard_valid == 0 && d.quiet == 0 && d.structural_fault == 1 &&
              d.protocol_fault == 1,
          "structural barrier blocks new requests, quiet, and recoverable clear");
    reset(d);
    check(d.structural_fault == 0 && d.protocol_fault == 0 && d.quiet == 1,
          "reset is the sole structural recovery");
  }
  {
    Dut d;
    reset(d);
    begin_geometry_data(d, kAssetBase + 0x2800u, 32);
    RawSeen final;
    int beats = 0;
    for (int beat = 0; beat < 16; ++beat) {
      RawSeen s = send_raw(d, static_cast<uint16_t>(0x8200 + beat), false);
      if (s.geom_valid) {
        ++beats;
        final = s;
      }
    }
    check(beats == 4 && final.geom_last,
          "missing physical LAST still emits only the exact fourth logical beat", 4, beats);
    check(d.structural_fault == 1 && d.protocol_faults == 1 && d.quiet == 0,
          "geometry beat16 without raw LAST latches structural barrier");
  }
}

void test_mixed_denial_counter_identity() {
  Dut d;
  reset(d);
  begin_texture_data(d, kAssetBase + 0x3000u);
  finish_texture_exact(d, 0x9100);
  begin_geometry_data(d, kAssetBase + 0x3200u, 32);
  finish_geometry_exact(d, 16, 0x9200);
  accept_geometry(d, kAssetBase, 16, be_for_len(16));
  consume_deny_verdict(d, true);
  check(d.guard_accepted == 3 && d.guard_ok_count == 2 && d.guard_denied == 1 &&
            d.geometry_accepted == 2 && d.texture_accepted == 1 && d.geometry_refused == 1 &&
            d.texture_refused_count == 0 && d.raw_halfwords == 24 && d.contention == 0 &&
            d.protocol_faults == 0 && d.quiet == 1,
        "mixed legal/denied drain has exact modulo counters and quiet", 3, d.guard_accepted);
  check(d.guard_accepted == d.guard_ok_count + d.guard_denied &&
            d.guard_accepted == d.geometry_accepted + d.texture_accepted,
        "mixed drain preserves both required counter partitions");
}

void run_hold_guard_valid_mutant() {
  Dut d;
  reset(d);
  const ReqSeen r = accept_texture(d, kAssetBase + 2u);
  check(r.client == kNone, "hold-valid inverse starts from one real NONE denial");
  d.eval();
  check(d.real_guard_violation == 1 && d.real_guard_ready == 1 && d.guard_valid == 1 &&
            d.texture_refused == 1,
        "mutant exact first signature: denial, ready-high, replayed valid, refusal");
  tick(d);  // first disposition and duplicate physical accept
  d.eval();
  check(d.real_guard_violation == 1 && d.guard_valid == 0,
        "duplicate accept creates the real guard's second denial");
  tick(d);  // classify second verdict as unsolicited; count second guard pulse
  d.eval();
  const bool exact = d.guard_accepted == 2 && d.guard_denied == 1 && d.guard_ok_count == 0 &&
                     d.texture_accepted == 2 && d.texture_refused_count == 1 &&
                     d.geometry_accepted == 0 && d.geometry_refused == 0 &&
                     d.guard_violations == 2 && d.raw_halfwords == 0 && d.contention == 0 &&
                     d.protocol_faults == 2 && d.protocol_fault == 1 && d.structural_fault == 0 &&
                     d.quiet == 1;
  check(exact,
        "hold-valid mutant exact signature is duplicate accept plus unsolicited verdict only");
}

void run_drift_subowner_mutant() {
  Dut d;
  reset(d);
  begin_texture_data(d, kAssetBase + 0x3400u);
  std::vector<RawSeen> seen;
  finish_texture_exact(d, 0xA100, &seen);
  int texture_words = 0;
  int geom_beats = 0;
  uint64_t geom_data = 0;
  bool geom_last = false;
  for (const RawSeen& s : seen) {
    texture_words += s.texture_valid;
    if (s.geom_valid) {
      ++geom_beats;
      geom_data = s.geom_data;
      geom_last = s.geom_last;
    }
  }
  const bool exact = texture_words == 4 && geom_beats == 1 &&
                     geom_data == packed4(0xA104, 0xA105, 0xA106, 0xA107) && geom_last &&
                     d.guard_accepted == 1 && d.guard_ok_count == 1 && d.guard_denied == 0 &&
                     d.texture_accepted == 1 && d.texture_refused_count == 0 &&
                     d.geometry_accepted == 0 && d.geometry_refused == 0 && d.raw_halfwords == 8 &&
                     d.protocol_faults == 4 && d.protocol_fault == 1 && d.structural_fault == 0 &&
                     d.contention == 0 && d.quiet == 1;
  check(exact, "drift mutant exact signature is 4 direct words then one packed geometry beat");
}

void run_texture_pack64_mutant() {
  Dut d;
  reset(d);
  begin_texture_data(d, kAssetBase + 0x3600u);
  std::vector<RawSeen> seen;
  finish_texture_exact(d, 0xA200, &seen);
  int texture_words = 0;
  std::vector<uint64_t> packed;
  int last_at = -1;
  for (const RawSeen& s : seen) {
    texture_words += s.texture_valid;
    if (s.geom_valid) {
      packed.push_back(s.geom_data);
      if (s.geom_last) last_at = static_cast<int>(packed.size());
    }
  }
  const bool exact = texture_words == 0 && packed.size() == 2 &&
                     packed[0] == packed4(0xA200, 0xA201, 0xA202, 0xA203) &&
                     packed[1] == packed4(0xA204, 0xA205, 0xA206, 0xA207) && last_at == 2 &&
                     d.guard_accepted == 1 && d.guard_ok_count == 1 && d.guard_denied == 0 &&
                     d.texture_accepted == 1 && d.texture_refused_count == 0 &&
                     d.geometry_accepted == 0 && d.geometry_refused == 0 && d.raw_halfwords == 8 &&
                     d.protocol_faults == 8 && d.protocol_fault == 1 && d.structural_fault == 0 &&
                     d.contention == 0 && d.quiet == 1;
  check(exact, "pack64 mutant exact signature is two geometry beats and zero direct words");
}

void run_terminate_beat7_mutant() {
  Dut d;
  reset(d);
  begin_texture_data(d, kAssetBase + 0x3800u);
  int delivered = 0;
  for (int beat = 0; beat < 7; ++beat) {
    const RawSeen s = send_raw(d, static_cast<uint16_t>(0xA300 + beat), beat == 6);
    delivered += s.texture_valid;
    check(!s.texture_refused && !s.geom_valid,
          "beat7 mutant changes no unrelated route/refusal output");
  }
  const bool exact =
      delivered == 7 && d.guard_accepted == 1 && d.guard_ok_count == 1 && d.guard_denied == 0 &&
      d.texture_accepted == 1 && d.texture_refused_count == 0 && d.geometry_accepted == 0 &&
      d.geometry_refused == 0 && d.raw_halfwords == 7 && d.protocol_faults == 1 &&
      d.protocol_fault == 1 && d.structural_fault == 0 && d.contention == 0 && d.quiet == 1;
  check(exact, "beat7 mutant exact signature releases after seven direct words only");
}

void run_accept_beat9_mutant() {
  Dut d;
  reset(d);
  begin_texture_data(d, kAssetBase + 0x3A00u);
  int delivered = 0;
  for (int beat = 0; beat < 9; ++beat) {
    const RawSeen s = send_raw(d, static_cast<uint16_t>(0xA400 + beat), beat == 8);
    delivered += s.texture_valid;
    check(!s.texture_refused && !s.geom_valid,
          "beat9 mutant changes no unrelated route/refusal output");
  }
  const bool exact =
      delivered == 9 && d.guard_accepted == 1 && d.guard_ok_count == 1 && d.guard_denied == 0 &&
      d.texture_accepted == 1 && d.texture_refused_count == 0 && d.geometry_accepted == 0 &&
      d.geometry_refused == 0 && d.raw_halfwords == 9 && d.protocol_faults == 2 &&
      d.protocol_fault == 1 && d.structural_fault == 0 && d.contention == 0 && d.quiet == 1;
  check(exact, "beat9 mutant exact signature accepts nine direct words before release");
}

void run_denial_silence_mutant() {
  Dut d;
  reset(d);
  const ReqSeen r = accept_texture(d, kAssetBase + 2u);
  check(r.client == kNone, "denial-silence inverse reaches a real NONE denial");
  d.eval();
  check(d.real_guard_violation == 1 && d.texture_refused == 0,
        "denial-silence mutant suppresses exactly the required local pulse");
  tick(d);
  d.eval();
  const bool exact = d.guard_accepted == 1 && d.guard_denied == 1 && d.guard_ok_count == 0 &&
                     d.texture_accepted == 1 && d.texture_refused_count == 0 &&
                     d.geometry_accepted == 0 && d.geometry_refused == 0 &&
                     d.guard_violations == 1 && d.raw_halfwords == 0 && d.protocol_faults == 1 &&
                     d.protocol_fault == 1 && d.structural_fault == 0 && d.contention == 0 &&
                     d.quiet == 1;
  check(exact, "denial-silence mutant exact signature is one denied job with no refusal");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#if defined(EXPECT_RENDER_ASSET_HOLD_GUARD_VALID_MUTANT)
  run_hold_guard_valid_mutant();
  report_and_exit("render_asset_mux_hold_guard_valid_mutant");
#elif defined(EXPECT_RENDER_ASSET_DRIFT_SUBOWNER_MUTANT)
  run_drift_subowner_mutant();
  report_and_exit("render_asset_mux_drift_subowner_mutant");
#elif defined(EXPECT_RENDER_ASSET_TEXTURE_PACK64_MUTANT)
  run_texture_pack64_mutant();
  report_and_exit("render_asset_mux_texture_pack64_mutant");
#elif defined(EXPECT_RENDER_ASSET_TERMINATE_BEAT7_MUTANT)
  run_terminate_beat7_mutant();
  report_and_exit("render_asset_mux_terminate_beat7_mutant");
#elif defined(EXPECT_RENDER_ASSET_ACCEPT_BEAT9_MUTANT)
  run_accept_beat9_mutant();
  report_and_exit("render_asset_mux_accept_beat9_mutant");
#elif defined(EXPECT_RENDER_ASSET_DENIAL_SILENCE_MUTANT)
  run_denial_silence_mutant();
  report_and_exit("render_asset_mux_denial_silence_mutant");
#else
  test_legal_16_32_64_and_counters();
  test_round_robin_both_histories_and_sustained_contention();
  test_boundaries_and_malformed_translation();
  test_guard_stall_hold_change_disappearance_and_clear();
  test_verdict_detectors_and_pre_ok_raw();
  test_texture_return_faults_and_drain();
  test_clear_set_priority_and_structural_geometry();
  test_mixed_denial_counter_identity();
  report_and_exit("render_asset_mux_directed");
#endif
}
