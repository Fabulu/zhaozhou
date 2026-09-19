// terrain_hdrread_directed.cpp -- TERRAIN.HDRREAD against spec/terrain_rules.md
// 2.1, and against the join fault it was built to remove.
//
// THE BLOCK. `zhao_console_core` entry I35 recorded TERRAIN.PLACE's
// `pitch_log2` and envelope as having no producer, and named the missing owner:
// a header reader on the COMPOSE path. This file is that block's acceptance.
//
// WHAT IS CHECKED, and the second half is the one that matters:
//
//   1. THE DECODE, against the spec's byte offsets written out here rather than
//      against a second transcription of the RTL's own beat cases. Every
//      expectation below is built by a helper that places a field at the offset
//      2.1 gives it, so a reader checks it against the spec.
//
//   2. THE ORDERING. The whole reason this is a block and not a wire is that
//      TERRAIN.SEQ's port ADVANCES while the header burst is in flight, and
//      anything read live off it afterwards belongs to the next patch. So one
//      test deliberately moves every job input to a different patch DURING the
//      burst and asserts that the header record and the forwarded job both
//      still carry the first one. That test fails on the obvious wrong
//      implementation and passes on this one, which is the only kind worth
//      writing here.
//
// AND EVERY COUNTER IS FIRED. CLAUDE.md: a detector reading zero is a claim and
// the claim to check hardest. `headers_read_o`, `headers_refused_o`,
// `guard_denied_o`, `incomplete_o` and `ident_fails_o` are each driven to move
// with stimulus that is LEGAL TO PRESENT and wrong in content -- a denied
// guard, a burst that ends early, a page whose header names another patch. None
// needs a mutant, because every refusal this block makes is reachable from its
// own two input ports.
//
// ONE JOB IN, ONE HEADER OUT, ONE JOB FORWARDED. Asserted after every case
// rather than once, because a refusal that swallowed the job would leave the
// page pinned in the directory forever and no other check here would notice.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"
#include "Vtb_terrain_hdrread.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

void check_true(bool ok, const char* what = "a required event happened") {
  check(ok, what, 1, ok ? 1 : 0);
}

// --------------------------------------------------------------------------
// The block's own constants, restated from its source of truth rather than
// from the RTL.
// --------------------------------------------------------------------------
constexpr uint32_t kPageBytes = 21376;          // terrain_rules 2 / 7
constexpr uint32_t kPoolBase = 0x0400'0000u;    // zhao_pkg ZHAO_TERRAIN_PAGE_POOL_BASE
constexpr uint32_t kPoolSlots = 1024;
constexpr int kClientTerrainBuild = 6;          // zhao_pkg ZHAO_CLIENT_TERRAIN_BUILD
constexpr int8_t kPitchRefuse = 127;            // not a pitch spec 1.3 can carry

constexpr int kVOk = 0;
constexpr int kVSlotOor = 1;
constexpr int kVEpoch = 2;
constexpr int kVGuard = 3;
constexpr int kVIncomplete = 4;
constexpr int kVIdent = 5;

// --------------------------------------------------------------------------
// THE PAGE HEADER, BUILT FROM spec/terrain_rules.md 2.1 BY OFFSET.
//
//     +0   u16 format_version
//     +2   i8  pitch_log2
//     +3   u8  flags
//     +4   u32 island_id
//     +8   i16 patch_ix, patch_iz
//     +12  u32 tileset_id
//     +16  rectfx envelope x0, z0, x1, z1
//     +32  u32 page_crc32c
//
// Little-endian, 64 bytes, returned as eight packed 64-bit beats. Written this
// way -- bytes at offsets -- so that the beat packing is the TEST's arithmetic
// and not a copy of the DUT's.
// --------------------------------------------------------------------------
struct Header {
  uint8_t b[64] = {};

  void put16(int off, uint16_t v) { b[off] = v & 0xff; b[off + 1] = (v >> 8) & 0xff; }
  void put32(int off, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[off + i] = (v >> (8 * i)) & 0xff;
  }
  uint64_t beat(int k) const {
    uint64_t w = 0;
    for (int i = 0; i < 8; ++i) w |= static_cast<uint64_t>(b[k * 8 + i]) << (8 * i);
    return w;
  }
};

Header make_header(uint16_t ver, int8_t pitch_log2, uint32_t island, int16_t ix, int16_t iz,
                   int32_t env_x0, int32_t env_z0) {
  Header h;
  h.put16(0, ver);
  h.b[2] = static_cast<uint8_t>(pitch_log2);
  h.b[3] = 0;                                  // flags
  h.put32(4, island);
  h.put16(8, static_cast<uint16_t>(ix));
  h.put16(10, static_cast<uint16_t>(iz));
  h.put32(12, 0xDEAD'BEEFu);                   // tileset_id: read and not exported
  h.put32(16, static_cast<uint32_t>(env_x0));
  h.put32(20, static_cast<uint32_t>(env_z0));
  h.put32(24, 0x1111'1111u);                   // envelope x1: ditto
  h.put32(28, 0x2222'2222u);                   // envelope z1
  h.put32(32, 0xC0FF'EE00u);                   // page_crc32c: PAGELOADER's business
  return h;
}

// --------------------------------------------------------------------------
// The job the compose door presents, held in one place so a test can move
// every field of it at once.
// --------------------------------------------------------------------------
struct Job {
  uint32_t slot = 0;
  uint32_t gen = 0;
  uint32_t epoch = 0;
  uint32_t src_id = 0;
  uint32_t flags = 0;
  uint32_t island = 0;
  int16_t ix = 0;
  int16_t iz = 0;
};

void drive_job(Vtb_terrain_hdrread& d, const Job& j) {
  d.j_slot = j.slot;
  d.j_gen = j.gen;
  d.j_epoch = j.epoch;
  d.j_src_id = j.src_id;
  d.j_flags = j.flags;
  d.j_island = j.island;
  d.j_ix = static_cast<uint16_t>(j.ix);
  d.j_iz = static_cast<uint16_t>(j.iz);
}

// --------------------------------------------------------------------------
// A MEM.GUARD MODEL WITH THE REAL PROTOCOL. `ready` is a LEVEL and `ok` is a
// PULSE one cycle after the accept -- `zhao_mem_guard.sv` says so outright, and
// a model that raised both together would let a DUT that tests them in one arm
// pass here and hang on the real guard. That is the exact defect
// tools/rtl/check_guard_verdict.py exists to catch, so the model must not be
// the forgiving version of it.
//
// `deny` refuses instead of passing; `short_at` ends the burst early on that
// beat index, which is the fabric giving up mid-transfer.
// --------------------------------------------------------------------------
struct Guard {
  const Header* page = nullptr;
  bool deny = false;
  int short_at = -1;          // beat index to assert `last` on, -1 = none

  // observations
  bool saw_request = false;
  uint32_t req_addr = 0;
  uint32_t req_len = 0;
  uint32_t req_client = 0;
  uint32_t req_write = 0;
  int requests = 0;

  // state
  int phase = 0;              // 0 idle, 1 verdict pending, 2 returning beats
  int beat = 0;

  void step(Vtb_terrain_hdrread& d) {
    d.g_ready = 0;
    d.g_ok = 0;
    d.g_violation = 0;
    d.beat_valid = 0;
    d.beat_last = 0;

    if (phase == 0) {
      if (d.g_valid) {
        saw_request = true;
        ++requests;
        req_addr = d.g_addr;
        req_len = d.g_len;
        req_client = d.g_client;
        req_write = d.g_write;
        d.g_ready = 1;        // the LEVEL
        phase = 1;
      }
    } else if (phase == 1) {
      if (deny) {
        d.g_violation = 1;    // the PULSE, one cycle later
        phase = 0;
      } else {
        d.g_ok = 1;
        beat = 0;
        phase = 2;
      }
    } else {
      d.beat_valid = 1;
      d.beat_data = page ? page->beat(beat) : 0;
      const bool last = (short_at >= 0) ? (beat == short_at) : (beat == 7);
      d.beat_last = last ? 1 : 0;
      if (last) phase = 0;
      ++beat;
    }
  }
};

void reset(Vtb_terrain_hdrread& d) {
  d.rst_n = 0;
  d.cfg_epoch = 0;
  d.j_valid = 0;
  drive_job(d, Job{});
  d.g_ready = 0;
  d.g_ok = 0;
  d.g_violation = 0;
  d.beat_valid = 0;
  d.beat_data = 0;
  d.beat_last = 0;
  d.h_ready = 0;
  d.f_ready = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

// What one patch produced.
struct Seen {
  bool header = false;
  int verdict = -1;
  int ok = -1;
  int8_t pitch = 0;
  int16_t ix = 0;
  int16_t iz = 0;
  int32_t env_x0 = 0;
  int32_t env_z0 = 0;
  uint32_t src_id = 0;

  bool forwarded = false;
  uint32_t f_slot = 0, f_gen = 0, f_epoch = 0, f_src = 0, f_flags = 0;
};

// Run one patch end to end: present the job, let the guard answer, take the
// header record and the forwarded job. `mutate_during_burst` is called once,
// on the cycle the guard starts returning beats, so a test can move the input
// port under the DUT.
Seen run_patch(Vtb_terrain_hdrread& d, Guard& g, const Job& j, int cycles = 80,
               void (*mutate_during_burst)(Vtb_terrain_hdrread&) = nullptr) {
  Seen s;
  drive_job(d, j);
  d.j_valid = 1;
  d.h_ready = 1;
  d.f_ready = 1;
  bool mutated = false;

  for (int c = 0; c < cycles; ++c) {
    g.step(d);

    // WHAT IS SAMPLED BEFORE THE EDGE AND WHAT IS CHANGED AFTER IT. Every
    // handshake below is read from the DUT's state-driven outputs as they stand
    // going INTO the rising edge, and `j_valid` is dropped only once that edge
    // has passed. Dropping it in place -- before `tick` -- retires the offer in
    // the same cycle the DUT was about to accept it, so the block never sees a
    // job at all. That is how the first run of this file reported 58 failures
    // with a correct DUT, and it is worth the comment: a driver that retires
    // its own request early looks exactly like a block that never starts.
    const bool accept_j = d.j_valid && d.j_ready;

    if (d.h_valid && d.h_ready && !s.header) {
      s.header = true;
      s.verdict = d.h_verdict;
      s.ok = d.h_ok;
      s.pitch = static_cast<int8_t>(d.h_pitch_log2);
      s.ix = static_cast<int16_t>(d.h_patch_ix);
      s.iz = static_cast<int16_t>(d.h_patch_iz);
      s.env_x0 = static_cast<int32_t>(d.h_env_x0);
      s.env_z0 = static_cast<int32_t>(d.h_env_z0);
      s.src_id = d.h_src_id;
    }
    if (d.f_valid && d.f_ready && !s.forwarded) {
      s.forwarded = true;
      s.f_slot = d.f_slot;
      s.f_gen = d.f_gen;
      s.f_epoch = d.f_epoch;
      s.f_src = d.f_src_id;
      s.f_flags = d.f_flags;
    }

    zhao::tick(d);

    if (accept_j) d.j_valid = 0;

    // The compose door moves on as soon as its ready came -- which is exactly
    // what TERRAIN.SEQ does, and exactly what this block has to be immune to.
    if (mutate_during_burst && !mutated && d.beat_valid) {
      mutate_during_burst(d);
      mutated = true;
    }

    if (s.header && s.forwarded) break;
  }
  return s;
}

const Job kJobA{/*slot=*/5, /*gen=*/0x21, /*epoch=*/0x1234, /*src_id=*/0xABCD1234,
                /*flags=*/0x0008, /*island=*/0x00C0FFEE, /*ix=*/3, /*iz=*/-7};

// ---------------------------------------------------------------------------
// 1. The decode, and the address the request was made at.
// ---------------------------------------------------------------------------
void test_header_decode_and_address() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  const Header page = make_header(1, /*pitch=*/1, kJobA.island, kJobA.ix, kJobA.iz,
                                  /*env_x0=*/0x0060'0000, /*env_z0=*/-0x000E'0000);
  Guard g;
  g.page = &page;

  const Seen s = run_patch(d, g, kJobA);

  check_true(s.header, "a header record was emitted");
  check(s.verdict == kVOk, "verdict is OK", kVOk, s.verdict);
  check(s.ok == 1, "h_ok_o is high on a clean header", 1, s.ok);
  check(s.pitch == 1, "pitch_log2 comes off header byte +2", 1, s.pitch);
  check(s.ix == kJobA.ix, "patch_ix comes off header byte +8", kJobA.ix, s.ix);
  check(s.iz == kJobA.iz, "patch_iz comes off header byte +10, sign preserved", kJobA.iz, s.iz);
  check(s.env_x0 == 0x0060'0000, "envelope x0 comes off header byte +16", 0x0060'0000, s.env_x0);
  check(s.env_z0 == -0x000E'0000, "envelope z0 comes off header byte +20, sign preserved",
        -0x000E'0000, s.env_z0);
  check(s.src_id == (kJobA.src_id & 0xffff), "the source id is the JOB's, narrowed",
        kJobA.src_id & 0xffff, s.src_id);

  // ONE burst, at the slot's page base, 64 bytes, read, as TERRAIN.BUILD.
  check(g.requests == 1, "exactly one guard request per patch", 1, g.requests);
  const uint32_t want = kPoolBase + kJobA.slot * kPageBytes;
  check(g.req_addr == want, "the request is at REGION_BASE + slot * PAGE_BYTES", want, g.req_addr);
  check(g.req_len == 64, "the request is 64 bytes -- the header is 64 bytes", 64, g.req_len);
  check(g.req_write == 0, "the request is a READ", 0, g.req_write);
  check(g.req_client == kClientTerrainBuild,
        "the request carries TERRAIN.BUILD, the only client the pool's window admits",
        kClientTerrainBuild, g.req_client);

  check_true(s.forwarded, "the job was forwarded to the streamer");
  check(s.f_slot == kJobA.slot, "the forwarded slot is the job's", kJobA.slot, s.f_slot);
  check(s.f_gen == kJobA.gen, "the forwarded generation is the job's", kJobA.gen, s.f_gen);
  check(s.f_epoch == kJobA.epoch, "the forwarded epoch is the job's", kJobA.epoch, s.f_epoch);
  check(s.f_src == kJobA.src_id, "the forwarded source id is the job's, WHOLE",
        kJobA.src_id, s.f_src);
  check(s.f_flags == kJobA.flags, "the forwarded flags are the job's -- kFlagDual rides here",
        kJobA.flags, s.f_flags);

  check(d.headers_read == 1, "headers_read_o fired", 1, d.headers_read);
  check(d.headers_refused == 0, "and nothing was refused", 0, d.headers_refused);
}

// ---------------------------------------------------------------------------
// 2. THE ORDERING TEST. This is the one that distinguishes this block from the
//    wire it replaces: the compose door's port MOVES while the burst is in
//    flight, and everything downstream must still see the first patch.
// ---------------------------------------------------------------------------
void test_the_job_port_may_advance_during_the_burst() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  const Header page = make_header(1, 1, kJobA.island, kJobA.ix, kJobA.iz, 0x0060'0000, 0);
  Guard g;
  g.page = &page;

  // TERRAIN.SEQ presents the NEXT patch as soon as its ready came. Every field
  // moves, including the ones the identity check uses.
  const Seen s = run_patch(d, g, kJobA, 80, [](Vtb_terrain_hdrread& dd) {
    dd.j_slot = 900;
    dd.j_gen = 0xFE;
    dd.j_epoch = 0x9999;
    dd.j_src_id = 0x55555555;
    dd.j_flags = 0xFFFF;
    dd.j_island = 0x0BADF00D;
    dd.j_ix = static_cast<uint16_t>(-321);
    dd.j_iz = 321;
  });

  check(s.verdict == kVOk,
        "the identity check used the LATCHED record, not the port that moved", kVOk, s.verdict);
  check(s.ix == kJobA.ix, "the header record still names the first patch", kJobA.ix, s.ix);
  check(s.src_id == (kJobA.src_id & 0xffff), "and carries the first patch's source id",
        kJobA.src_id & 0xffff, s.src_id);
  check(s.f_slot == kJobA.slot,
        "the FORWARDED job carries the first patch's slot -- the streamer would otherwise "
        "read page N's bytes under page M's identity",
        kJobA.slot, s.f_slot);
  check(s.f_gen == kJobA.gen, "...its generation", kJobA.gen, s.f_gen);
  check(s.f_epoch == kJobA.epoch, "...its epoch", kJobA.epoch, s.f_epoch);
  check(s.f_flags == kJobA.flags, "...and its flags", kJobA.flags, s.f_flags);

  const uint32_t want = kPoolBase + kJobA.slot * kPageBytes;
  check(g.req_addr == want, "and the burst was issued at the first patch's page base",
        want, g.req_addr);
}

// ---------------------------------------------------------------------------
// 3. A header that names another patch. `ident_fails_o` fires, the pitch is
//    poisoned, and the job is STILL forwarded so the page can unpin.
// ---------------------------------------------------------------------------
void test_identity_mismatch_fires_and_poisons_the_pitch() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  // Everything right except the island: the page in this slot belongs to
  // somebody else, which is what the format's redundancy exists to catch.
  const Header page = make_header(1, 1, kJobA.island ^ 0xFFu, kJobA.ix, kJobA.iz, 0x0060'0000, 0);
  Guard g;
  g.page = &page;

  const Seen s = run_patch(d, g, kJobA);

  check(s.verdict == kVIdent, "verdict is IDENT", kVIdent, s.verdict);
  check(s.ok == 0, "h_ok_o is low", 0, s.ok);
  check(d.ident_fails == 1, "ident_fails_o fired", 1, d.ident_fails);
  check(s.pitch == kPitchRefuse,
        "the pitch is the poison value, so TERRAIN.PLACE refuses on its own law",
        kPitchRefuse, s.pitch);
  check(s.env_x0 == 0, "and the envelope is not forwarded from an unbelieved header", 0, s.env_x0);
  check_true(s.forwarded);
  check(d.headers_refused == 1, "headers_refused_o fired", 1, d.headers_refused);
  check(d.headers_read == 0, "and headers_read_o did not", 0, d.headers_read);

  // A WRONG FORMAT VERSION IS THE SAME REFUSAL, checked here rather than in its
  // own case because it is the same comparison.
  const Header v2 = make_header(2, 1, kJobA.island, kJobA.ix, kJobA.iz, 0x0060'0000, 0);
  g.page = &v2;
  const Seen s2 = run_patch(d, g, kJobA);
  check(s2.verdict == kVIdent, "a format_version that is not 1 is refused", kVIdent, s2.verdict);
  check(d.ident_fails == 2, "ident_fails_o fired again", 2, d.ident_fails);
}

// ---------------------------------------------------------------------------
// 4. The guard denies. `guard_denied_o` fires and the page still streams.
// ---------------------------------------------------------------------------
void test_guard_denial_fires() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  Guard g;
  g.deny = true;

  const Seen s = run_patch(d, g, kJobA);

  check(s.verdict == kVGuard, "verdict is GUARD", kVGuard, s.verdict);
  check(d.guard_denied == 1, "guard_denied_o fired", 1, d.guard_denied);
  check(s.pitch == kPitchRefuse, "the pitch is poisoned", kPitchRefuse, s.pitch);
  check(s.ix == kJobA.ix,
        "the coordinate falls back to the JOB's record rather than to whatever was in the "
        "capture registers",
        kJobA.ix, s.ix);
  check_true(s.forwarded, "the page is still streamed, so it still unpins");
}

// ---------------------------------------------------------------------------
// 5. The burst ends early. A header cut off before beat 2 has NO envelope, and
//    a missing envelope reads as the world origin -- a real place.
// ---------------------------------------------------------------------------
void test_short_burst_fires() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  const Header page = make_header(1, 1, kJobA.island, kJobA.ix, kJobA.iz, 0x0060'0000, 0);
  Guard g;
  g.page = &page;
  g.short_at = 1;             // `last` on beat 1: the envelope never arrives

  const Seen s = run_patch(d, g, kJobA);

  check(s.verdict == kVIncomplete, "verdict is INCOMPLETE", kVIncomplete, s.verdict);
  check(d.incomplete == 1, "incomplete_o fired", 1, d.incomplete);
  check(s.pitch == kPitchRefuse,
        "a short burst poisons the pitch rather than placing from half a header",
        kPitchRefuse, s.pitch);
  check_true(s.forwarded);
}

// ---------------------------------------------------------------------------
// 6. THE REFUSAL MUST NOT ISSUE THE READ IT IS REFUSING. A slot outside the
//    pool and a stale epoch are both decided before any request, so
//    `guard_denied_o` stays a measurement of the GUARD rather than of this
//    block's own bookkeeping.
// ---------------------------------------------------------------------------
void test_prechecks_issue_no_request() {
  {
    Vtb_terrain_hdrread d;
    reset(d);
    d.cfg_epoch = kJobA.epoch;
    Guard g;
    Job j = kJobA;
    j.slot = kPoolSlots;      // the extra refusal bit: slot 1,024 of 1,024
    const Seen s = run_patch(d, g, j);
    check(s.verdict == kVSlotOor, "an out-of-pool slot is SLOT_OOR", kVSlotOor, s.verdict);
    check(g.requests == 0, "and no guard request was issued at all", 0, g.requests);
    check(d.guard_denied == 0, "so guard_denied_o stays a measurement of the guard",
          0, d.guard_denied);
    check_true(s.forwarded, "the job is still forwarded");
  }
  {
    Vtb_terrain_hdrread d;
    reset(d);
    d.cfg_epoch = kJobA.epoch + 1;   // the live epoch has moved on
    Guard g;
    const Seen s = run_patch(d, g, kJobA);
    check(s.verdict == kVEpoch, "a stale job epoch is EPOCH", kVEpoch, s.verdict);
    check(g.requests == 0, "and no guard request was issued", 0, g.requests);
    check_true(s.forwarded);
  }
}

// ---------------------------------------------------------------------------
// 7. Conservation across a mixed run. One job in, one header out, one job
//    forwarded -- whatever the verdicts were.
// ---------------------------------------------------------------------------
void test_one_job_one_header_one_forward() {
  Vtb_terrain_hdrread d;
  reset(d);
  d.cfg_epoch = kJobA.epoch;

  const Header good = make_header(1, 1, kJobA.island, kJobA.ix, kJobA.iz, 0x0060'0000, 0);
  const Header wrong = make_header(1, 1, kJobA.island + 1, kJobA.ix, kJobA.iz, 0x0060'0000, 0);

  Guard g;
  int headers = 0, forwards = 0;
  for (int n = 0; n < 6; ++n) {
    g.deny = (n == 2);
    g.short_at = (n == 4) ? 3 : -1;
    g.page = ((n % 3) == 1) ? &wrong : &good;
    Job j = kJobA;
    j.slot = 1 + n;
    const Seen s = run_patch(d, g, j);
    if (s.header) ++headers;
    if (s.forwarded) ++forwards;
    check(s.f_slot == j.slot, "each forward carries its own job's slot", j.slot, s.f_slot);
  }

  check(headers == 6, "six jobs produced six header records", 6, headers);
  check(forwards == 6, "and six forwarded jobs", 6, forwards);
  const uint32_t total = d.headers_read + d.headers_refused;
  check(total == 6, "read + refused accounts for every job", 6, total);
  check(d.idle == 1, "the block is idle at the end", 1, d.idle);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_header_decode_and_address();
  test_the_job_port_may_advance_during_the_burst();
  test_identity_mismatch_fires_and_poisons_the_pitch();
  test_guard_denial_fires();
  test_short_burst_fires();
  test_prechecks_issue_no_request();
  test_one_job_one_header_one_forward();

  std::printf("terrain_hdrread_directed: %d checks, %d failed\n", g_checks, g_failed);
  std::fflush(stdout);
  // TEARDOWN-DEADLOCK WORKAROUND, documented in tests/harness/zhao_sim.hpp: a
  // plain C++ return is exactly the shape that hangs under ctest at ~0 CPU.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
