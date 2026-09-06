// island_dir_rtl_directed.cpp -- the island directory RTL against its oracle.
//
// TERRAIN.ISLAND's reference model is `zref::island::Directory`, and this
// compares the hardware against it rather than against a second transcription
// of the outcome rules. The block's entire job is a MAPPING -- from a store's
// hit or miss, plus two gates, to one of four answers -- so a test that checked
// only "an answer came back" would check nothing at all.
//
// THE ANSWER THAT MATTERS IS OPEN SKY. An 8 km island at the canonical pitch is
// 125 x 125 = 15,625 patches, of which about 793 are ground -- 5.1%. So 94.9%
// of every honest query legitimately finds nothing, and the whole reason this
// block exists is that the store underneath cannot know the difference between
// "no ground here" and "something went wrong". Both would be a miss.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_island_dir.h"

#include "zhao_sim.hpp"
#include "zref/zref_island.hpp"

namespace isl = zref::island;

namespace {

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
  }
}

constexpr int32_t kSide = 125;  // 8 km at 64 m patches

// The outcome encoding must match zref::island::Outcome, and this is where a
// silent divergence would hide: the RTL's localparams and the enum are two
// declarations of one law.
constexpr int kResident = 0, kOpenSky = 1, kOutOfExtent = 2, kBadPitch = 3;

int oracle_code(isl::Outcome o) {
  switch (o) {
    case isl::Outcome::kResident: return kResident;
    case isl::Outcome::kOpenSky: return kOpenSky;
    case isl::Outcome::kOutOfExtent: return kOutOfExtent;
    default: return kBadPitch;
  }
}

struct Query {
  int32_t ix, iz;
  uint8_t tag;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_island_dir d;

  // ---- the island, and the same solid band the 8 km reference test uses ---
  isl::Desc desc;
  desc.island_id = 0x515Au;
  desc.pitch_log2 = 1;  // canonical 2.0 m -> 64 m patch
  desc.extent_ix = static_cast<uint16_t>(kSide);
  desc.extent_iz = static_cast<uint16_t>(kSide);

  // THE SHAPED ISLAND THE SPEC COSTS OUT, not a rectangle. A floating island is
  // not a square, and the corners are exactly the sky that must cost nothing:
  //
  //   3.25 km^2 / (64 m)^2 = 793.5 patches  ->  radius = sqrt(793.5/pi) = 15.9
  //
  // Copied deliberately from island_8km_directed so the two tests describe ONE
  // island. My first version used the stream test's 45-row band -- 5,625
  // patches -- and then asserted the 793 figure against it, which is a
  // measurement taken across two different fixtures and exactly the mistake
  // this project keeps writing down.
  isl::Directory dir(desc);
  uint32_t h = 1;
  {
    const double radius = 15.9;
    const int32_t cx = kSide / 2, cz = kSide / 2;
    for (int32_t iz = 0; iz < kSide; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix) {
        const double dx = ix - cx, dz = iz - cz;
        if (dx * dx + dz * dz <= radius * radius) dir.set(ix, iz, h++);
      }
  }

  d.rst_n = 0;
  d.q_valid = 0;
  d.gw_en = 0;
  d.a_ready = 1;
  d.desc_extent_ix = static_cast<uint16_t>(kSide);
  d.desc_extent_iz = static_cast<uint16_t>(kSide);
  d.desc_pitch_log2 = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;

  // ---- load the modelled store with exactly the same ground ---------------
  h = 1;
  {
    const double radius = 15.9;
    const int32_t cx = kSide / 2, cz = kSide / 2;
    for (int32_t iz = 0; iz < kSide; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix) {
        const double dx = ix - cx, dz = iz - cz;
        if (dx * dx + dz * dz > radius * radius) continue;
        d.gw_en = 1;
        d.gw_addr = static_cast<uint16_t>((iz << 7) | ix);
        d.gw_handle = h++;
        zhao::tick(d);
      }
  }
  d.gw_en = 0;
  zhao::tick(d);

  // ---- the queries --------------------------------------------------------
  // Deliberately mixed so no outcome can be produced by a stuck answer, and
  // ordered so a block that answered the PREVIOUS query would be caught by the
  // tag comparison.
  std::vector<Query> qs;
  uint8_t tag = 0;
  for (int32_t iz = 45; iz < 80; ++iz) {          // crosses the disc's edges
    qs.push_back({60, iz, tag++});
    qs.push_back({0, iz, tag++});
    qs.push_back({kSide - 1, iz, tag++});
  }
  qs.push_back({-1, 60, tag++});                   // negative: outside, not huge
  qs.push_back({60, -1, tag++});
  qs.push_back({kSide, 60, tag++});                // one past the extent
  qs.push_back({60, kSide, tag++});
  qs.push_back({100000, 60, tag++});

  std::vector<Query> rq;
  isl::Ledger oracle_led{};
  int mismatched = 0, handle_bad = 0, tag_bad = 0, compared = 0;
  std::size_t next = 0;
  std::vector<int> got_codes;

  for (int cyc = 0; cyc < 200000 && (next < qs.size() || got_codes.size() < qs.size()); ++cyc) {
    d.q_valid = (next < qs.size()) ? 1 : 0;
    if (next < qs.size()) {
      d.q_ix = static_cast<uint32_t>(qs[next].ix);
      d.q_iz = static_cast<uint32_t>(qs[next].iz);
      d.q_tag = qs[next].tag;
    }
    d.a_ready = 1;
    d.eval();

    const bool took = d.q_valid && d.q_ready;
    if (d.a_valid && d.a_ready) {
      const std::size_t i = got_codes.size();
      if (i < qs.size()) {
        const isl::Lookup want = dir.find(qs[i].ix, qs[i].iz, &oracle_led);
        const int want_code = oracle_code(want.outcome);
        ++compared;
        if (static_cast<int>(d.a_outcome) != want_code) {
          if (mismatched < 3)
            std::printf("    (%d,%d): rtl %d, oracle %d\n", qs[i].ix, qs[i].iz,
                        static_cast<int>(d.a_outcome), want_code);
          ++mismatched;
        }
        // THE HANDLE IS ONLY MEANINGFUL ON A HIT, and the reference returns 0
        // otherwise -- so comparing it unconditionally also proves the block
        // does not leak a stale handle into a sky answer.
        if (d.a_handle != want.page_handle) ++handle_bad;
        if (d.a_tag != qs[i].tag) ++tag_bad;
      }
      got_codes.push_back(static_cast<int>(d.a_outcome));
    }
    zhao::tick(d);
    if (took) ++next;
  }

  std::printf("  %zu queries, %d compared: rtl counters resident %u sky %u out %u badpitch %u\n",
              qs.size(), compared, d.cnt_resident, d.cnt_open_sky, d.cnt_out_of_extent,
              d.cnt_bad_pitch);
  std::printf("  oracle ledger:              resident %u sky %u out %u badpitch %u\n",
              oracle_led.resident, oracle_led.open_sky, oracle_led.out_of_extent,
              oracle_led.bad_pitch);

  ck(got_codes.size() == qs.size(), "every query was answered exactly once",
     static_cast<long long>(qs.size()), static_cast<long long>(got_codes.size()));
  ck(mismatched == 0,
     "every outcome matches zref::island::Directory::find -- resident, open "
     "sky, out of extent and bad pitch, from the oracle rather than from a "
     "second copy of the rules",
     0, mismatched);
  ck(handle_bad == 0,
     "and every page handle matches, including the zero the reference returns "
     "for a non-resident answer -- so a sky answer cannot carry a stale handle",
     0, handle_bad);
  ck(tag_bad == 0,
     "with each answer carrying its OWN query's tag, so a block answering the "
     "previous query could not pass",
     0, tag_bad);

  // The counters are the block's evidence ports and must agree with the
  // oracle's ledger, not merely be non-zero.
  ck(d.cnt_resident == oracle_led.resident, "the resident counter matches the oracle's ledger",
     oracle_led.resident, d.cnt_resident);
  ck(d.cnt_open_sky == oracle_led.open_sky, "and the open-sky counter", oracle_led.open_sky,
     d.cnt_open_sky);
  ck(d.cnt_out_of_extent == oracle_led.out_of_extent, "and out-of-extent",
     oracle_led.out_of_extent, d.cnt_out_of_extent);

  // NOT VACUOUS: the run must actually contain each interesting answer.
  ck(oracle_led.resident > 0 && oracle_led.open_sky > 0 && oracle_led.out_of_extent > 0,
     "and the run genuinely contained resident, sky AND out-of-extent answers, "
     "so the comparison above is not satisfied by one outcome repeated",
     1, (oracle_led.resident > 0 && oracle_led.open_sky > 0 && oracle_led.out_of_extent > 0) ? 1
                                                                                            : 0);
  // THE SKY-IS-COMMONEST CLAIM NEEDS THE WHOLE GRID, NOT THIS QUERY SET.
  //
  // My first version asserted `open_sky > resident` right here, on the directed
  // queries above -- and it failed, correctly. Those queries deliberately walk
  // the ground band (iz 38..88 straddles the solid rows 40..84), so 135 of 155
  // land on ground. That is a fine set for checking OUTCOMES and terrible for
  // checking a RATIO: it asserts a property of the island's grid from a sample
  // chosen for a different purpose entirely.
  //
  // The 94.9% figure is about the grid, so the grid is what has to be swept.
  {
    std::size_t next2 = 0, answered = 0;
    uint32_t rtl_res = d.cnt_resident, rtl_sky = d.cnt_open_sky;
    isl::Ledger full{};
    const int32_t total = kSide * kSide;

    for (int cyc = 0; cyc < 4000000 && answered < static_cast<std::size_t>(total); ++cyc) {
      const bool more = next2 < static_cast<std::size_t>(total);
      d.q_valid = more ? 1 : 0;
      if (more) {
        d.q_ix = static_cast<uint32_t>(static_cast<int32_t>(next2) % kSide);
        d.q_iz = static_cast<uint32_t>(static_cast<int32_t>(next2) / kSide);
        d.q_tag = static_cast<uint8_t>(next2 & 0xFF);
      }
      d.a_ready = 1;
      d.eval();
      const bool took = d.q_valid && d.q_ready;
      if (d.a_valid && d.a_ready) {
        const int32_t i = static_cast<int32_t>(answered);
        dir.find(i % kSide, i / kSide, &full);
        ++answered;
      }
      zhao::tick(d);
      if (took) ++next2;
    }

    const uint32_t swept_res = d.cnt_resident - rtl_res;
    const uint32_t swept_sky = d.cnt_open_sky - rtl_sky;
    std::printf("  full grid sweep: %d patches -> rtl resident %u sky %u | oracle %u / %u\n",
                total, swept_res, swept_sky, full.resident, full.open_sky);

    ck(answered == static_cast<std::size_t>(total), "the whole grid was swept",
       total, static_cast<long long>(answered));
    ck(swept_res == full.resident && swept_sky == full.open_sky,
       "and over the WHOLE grid the counters still match the oracle exactly",
       full.resident, swept_res);
    ck(swept_res == 793,
       "with 793 patches of ground -- the figure terrain_rules 1.4 costs out "
       "from the island's 3.25 square km, derived independently",
       793, swept_res);
    ck(swept_sky > swept_res * 10,   // 14,832 / 793 = 18.7x
       "and SKY more than ten times commoner than ground, which is the property "
       "the whole block exists for: a store whose miss meant failure would "
       "report the overwhelming majority of an island as broken",
       1, swept_sky > swept_res * 10 ? 1 : 0);
  }

  // ---- a malformed descriptor -------------------------------------------
  // BAD PITCH OUTRANKS OUT OF EXTENT, and the order is load-bearing: a
  // descriptor that names a pitch the machine does not have cannot be trusted
  // to say what its extent is either.
  {
    isl::Desc bad = desc;
    bad.pitch_log2 = 7;  // not in {-1, 0, 1, 2}
    isl::Directory bdir(bad);
    isl::Ledger bl{};
    const isl::Lookup want = bdir.find(100000, 60, &bl);

    d.desc_pitch_log2 = 7;
    d.q_valid = 1;
    d.q_ix = static_cast<uint32_t>(100000);
    d.q_iz = 60;
    d.q_tag = 0xAB;
    int seen = -1;
    for (int cyc = 0; cyc < 1000 && seen < 0; ++cyc) {
      d.a_ready = 1;
      d.eval();
      if (d.a_valid) seen = static_cast<int>(d.a_outcome);
      const bool took = d.q_valid && d.q_ready;
      zhao::tick(d);
      if (took) d.q_valid = 0;
    }
    ck(seen == oracle_code(want.outcome),
       "an out-of-extent query on an ILLEGAL PITCH reports BAD PITCH, not out "
       "of extent -- a malformed descriptor cannot be trusted to say what its "
       "extent is",
       oracle_code(want.outcome), seen);
    ck(seen == kBadPitch, "which is BAD_PITCH", kBadPitch, seen);
    ck(d.cnt_bad_pitch > 0, "and it is counted", 1, static_cast<long long>(d.cnt_bad_pitch));
  }

  // ======================= RANDOMISED, AND NOT DECORATIVE =================
  // The directed cases above pick their coordinates to exercise named edges.
  // This DRAWS them, including out-of-range and negative ones, so the outcome
  // mapping is checked on inputs nobody chose. A deterministic generator, so a
  // failure is reproducible.
  {
    // RESTORE THE DESCRIPTOR. The bad-pitch scenario above deliberately set an
    // illegal pitch and did not put it back, so every draw here returned
    // BAD_PITCH while the oracle -- holding a legal descriptor -- returned
    // something else. All 3,000 mismatched, which is the shape of a systematic
    // setup error rather than a defect: a real mapping bug does not miss
    // everything.
    d.desc_pitch_log2 = 1;

    uint32_t st = 0x151A4Du;
    auto nxt = [&st]() {
      st = st * 1664525u + 1013904223u;
      return st;
    };
    const int kN = 3000;
    int rnd_bad = 0, rnd_handle_bad = 0, rnd_tag_bad = 0;
    isl::Ledger rl{};
    int sent = 0, seen = 0;

    // A DRAW IS HELD UNTIL IT IS ACCEPTED. Redrawing the coordinate every
    // cycle while q_valid is asserted would break the ready/valid contract --
    // the payload must be stable across a stall -- and it also desynchronises
    // the expected-answer list from what the block actually consumed. The
    // first version of this loop did both, and 907 of 3,000 "mismatched": the
    // 37 resident plus the 870 sky, i.e. every in-extent draw. A defect that
    // lands on exactly one clean partition of the input is a bench artefact.
    bool have_draw = false;
    int32_t qx = 0, qz = 0;
    uint8_t qtag = 0;

    for (int cyc = 0; cyc < 4000000 && seen < kN; ++cyc) {
      if (!have_draw && sent < kN) {
        // A quarter of the draws land outside the extent on purpose, split
        // between NEGATIVE and BEYOND -- the two ways a coordinate can be out,
        // and the negative one is where an unsigned compare would wrap it into
        // the middle of the grid instead of rejecting it.
        const uint32_t r = nxt();
        qx = ((r & 3u) == 0u) ? (-static_cast<int32_t>((r >> 8) % 500u) - 1)
                              : static_cast<int32_t>((r >> 8) % 160u);
        const uint32_t r2 = nxt();
        qz = ((r2 & 3u) == 0u) ? (-static_cast<int32_t>((r2 >> 8) % 500u) - 1)
                               : static_cast<int32_t>((r2 >> 8) % 160u);
        qtag = static_cast<uint8_t>(sent & 0xFF);
        have_draw = true;
      }

      d.q_valid = have_draw ? 1 : 0;
      d.q_ix = static_cast<uint32_t>(qx);
      d.q_iz = static_cast<uint32_t>(qz);
      d.q_tag = qtag;
      d.a_ready = 1;
      d.eval();

      const bool took = d.q_valid && d.q_ready;
      if (d.a_valid && d.a_ready) {
        if (seen < static_cast<int>(rq.size())) {
          const isl::Lookup want = dir.find(rq[seen].ix, rq[seen].iz, &rl);
          if (static_cast<int>(d.a_outcome) != oracle_code(want.outcome)) ++rnd_bad;
          if (d.a_handle != want.page_handle) ++rnd_handle_bad;
          if (d.a_tag != rq[seen].tag) ++rnd_tag_bad;
        }
        ++seen;
      }
      zhao::tick(d);
      if (took) {
        rq.push_back({qx, qz, qtag});
        have_draw = false;
        ++sent;
      }
    }

    std::printf("  randomised: %d draws -> resident %u sky %u out %u\n", seen, rl.resident,
                rl.open_sky, rl.out_of_extent);
    ck(seen == kN, "every random draw was answered", kN, seen);
    ck(rnd_bad == 0, "and every randomised outcome matches the oracle", 0, rnd_bad);
    ck(rnd_handle_bad == 0, "including its page handle", 0, rnd_handle_bad);
    ck(rnd_tag_bad == 0, "and the answer carries its own query's tag", 0, rnd_tag_bad);
    ck(seen == static_cast<int>(rq.size()),
       "with exactly one answer per accepted query, so the comparison above was "
       "aligned rather than merely the same length",
       static_cast<int>(rq.size()), seen);
    ck(rl.out_of_extent > 0 && rl.resident > 0 && rl.open_sky > 0,
       "with all three reachable outcomes actually drawn, so this is not a "
       "randomised walk over one answer",
       1, (rl.out_of_extent > 0 && rl.resident > 0 && rl.open_sky > 0) ? 1 : 0);
  }

  if (g_fail) {
    std::printf("[island_dir_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[island_dir_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
