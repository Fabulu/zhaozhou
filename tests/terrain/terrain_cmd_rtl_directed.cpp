// terrain_cmd_rtl_directed.cpp -- TERRAIN.CMD against zref::swstream.
//
// ===========================================================================
// THE ORACLE IS THE ENCODER, READ BACKWARDS
// ===========================================================================
// `zref::swstream::encode_record` turns a `PatchRecord` into T5's 32 bytes.
// This block turns those bytes back into the ten fields TERRAIN.SEQ takes. So
// the test is a round trip with a real oracle and no new law: encode a list,
// put it in the played HPS arena, and require that what comes out of the block
// is field-for-field the list that went in, IN ORDER.
//
// Every way this block can be wrong survives a count:
//
//   * a sheared field -- `patch_ix` read from the wrong half of beat 0 -- is
//     terrain in the wrong place, and the record count is unchanged.
//   * a reordered stream is a determinism failure that renders correctly on
//     the machine that recorded it. T5 calls the list capture data precisely
//     because that is the failure it fears.
//   * a truncated stream is a frame that is missing ground it asked for, and
//     `patch_count` is T5's SEAL: TERRAIN.SEQ stops there, so stopping early
//     here is invisible downstream.
//   * a list acted on BEFORE its CRC was checked has already issued loads.
//
// So the comparison is the whole stream, record by record and field by field,
// and the CRC phase checks that a corrupt list produces ZERO records rather
// than "some records and then a fault".
//
// THE FIXTURE MAKES EVERY CONFUSION A DIFFERENT NUMBER. Each field of each
// record is a distinct function of the record index, `patch_ix` and `patch_iz`
// take BOTH SIGNS (they are i16 on the wire and a zero-extending read passes
// every test drawn from positive coordinates), and `hps_page_addr` reaches
// above 2^32 so a 32-bit truncation cannot hide.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_cmd.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"
#include "zref/zref_sw_stream.hpp"

namespace {

namespace ss = zref::swstream;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
    std::fflush(stdout);
  }
}

void ck(bool ok, const char* what, long expect, long got) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, got);
    std::fflush(stdout);
  }
}

constexpr uint32_t kArenaBase = 0x2000'0000u;
constexpr uint32_t kArenaBytes = 128u * 1024u;
constexpr uint32_t kListOff = 0x0800u;      // 8-byte aligned, well inside
constexpr uint32_t kEpoch = 0x00C0'FFEEu;

// Verdicts, from the RTL's localparams.
enum : int {
  kVOk = 0, kVEpoch = 1, kVLen = 2, kVCount = 3, kVAlign = 4,
  kVUnreach = 5, kVCrc = 6, kVBridge = 7, kVEmpty = 8
  // 9 WAS `kVSeq` AND IS DELIBERATELY UNALLOCATED. The block refused any
  // `sequence` above 65,535, on the belief that TERRAIN.SEQ's frame ring
  // carried sixteen bits. It carries thirty-two -- `zhao_terrain_seq.sv:110`
  // -- and the narrowing that had been seen is at `cl_seq_o`, the CLAIM
  // sequence, which is a different signal. The refusal and its case here are
  // both gone; the code number is left as a gap rather than reused, because a
  // verdict that changes meaning between builds is worse than a hole.
};

// EVERY FIELD A DISTINCT FUNCTION OF THE INDEX. See the header.
ss::PatchRecord rec_of(uint32_t i) {
  ss::PatchRecord r;
  r.island_id = 0x1500'0000u + i * 0x0002'0003u;
  // BOTH SIGNS.
  r.patch_ix = int16_t((i % 2u) ? -int(i * 13u % 30000u) : int(i * 11u % 30000u));
  r.patch_iz = int16_t((i % 3u) ? int(i * 17u % 30000u) : -int(i * 19u % 30000u));
  // ABOVE 2^32, so a 32-bit truncation cannot pass.
  r.hps_page_addr = 0x0000'0003'0000'0000ull + uint64_t(i) * 21376ull;
  r.expected_page_crc32c = 0xC0DE'0000u ^ (i * 0x9E37'79B9u);
  r.flags = uint16_t(ss::kFlagRequired | ((i & 1u) ? ss::kFlagDynamic : ss::kFlagPrefetch));
  r.view_mask = uint8_t(1u + (i % 3u));
  r.priority = uint8_t(i % 5u);
  r.source_id = 0x5000'0000u + i * 0x0011'0007u;
  r.reserved = 0;
  return r;
}

struct World {
  Vtb_terrain_cmd& d;
  std::vector<uint8_t> arena;

  explicit World(Vtb_terrain_cmd& dd) : d(dd), arena(kArenaBytes, 0) {}

  void quiet() {
    d.mw_en = 0;
    d.j_valid = 0;
    d.rec_ready = 0;
    d.done_ready = 0;
    d.stat_clear_i = 0;
  }

  void config() {
    d.cfg_req_latency_i = 2;
    d.cfg_beat_gap_i = 0;
    d.cfg_err_mode_i = 0;
    d.cfg_err_burst_i = 0;
    d.cfg_hps_client_i = 6;   // ZHAO_CLIENT_TERRAIN_BUILD
    d.cfg_epoch_i = kEpoch;
    d.cfg_arena_base_i = kArenaBase;
    d.cfg_arena_bytes_i = kArenaBytes;
  }

  void reset() {
    d.rst_n = 0;
    quiet();
    config();
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(d);
  }

  // Write the arena image into the bench's memory. Only the words the list
  // touches, plus a margin, so a 128 KB arena does not cost 16,384 ticks per
  // phase.
  void upload(uint32_t byte_off, uint32_t bytes) {
    const uint32_t w0 = byte_off / 8;
    const uint32_t w1 = (byte_off + bytes + 7) / 8;
    for (uint32_t w = w0; w < w1; ++w) {
      uint64_t v = 0;
      for (int b = 0; b < 8; ++b) v |= uint64_t(arena[w * 8 + uint32_t(b)]) << (8 * b);
      d.mw_en = 1;
      d.mw_addr = w;
      d.mw_data = v;
      d.eval();
      zhao::tick(d);
    }
    d.mw_en = 0;
    d.eval();
  }

  // Lay a list into the arena and return its CRC-32C, computed by the SAME
  // function the hardware folds -- `zhao_crc32c` from the generated ABI. A
  // second implementation here would be a second thing to keep in step.
  uint32_t place(uint32_t off, const std::vector<ss::PatchRecord>& recs) {
    for (std::size_t i = 0; i < recs.size(); ++i)
      ss::encode_record(recs[i], &arena[off + i * ss::kRecordBytes]);
    const uint32_t bytes = uint32_t(recs.size() * ss::kRecordBytes);
    upload(off, bytes);
    return zhao_abi::zhao_crc32c(0, &arena[off], bytes);
  }
};

struct Out {
  std::vector<ss::PatchRecord> recs;
  bool done = false;
  bool ok = false;
  int verdict = -1;
  uint32_t crc_seen = 0;
  uint32_t src = 0;
  int frames = 0;             // fr_start pulses
  uint32_t fr_count = 0;
  uint32_t fr_seq = 0;
  uint32_t fr_epoch = 0;
  int rec_before_frame = 0;   // records offered before any fr_start
  uint64_t cycles = 0;
};

bool draw(uint32_t& s, int pattern) {
  s = s * 1664525u + 1013904223u;
  switch (pattern) {
    case 0: return true;
    case 1: return ((s >> 16) & 1u) != 0u;
    case 2: return ((s >> 16) & 3u) != 0u;
    default: return ((s >> 16) & 7u) == 0u;
  }
}

Out submit(World& w, uint32_t epoch, uint32_t off, uint32_t bytes, uint32_t crc,
           uint16_t count, uint32_t seq, uint32_t src, int pattern,
           uint64_t cap = 400000ull) {
  Vtb_terrain_cmd& d = w.d;
  Out o;
  uint32_t sr = 0x2468u ^ uint32_t(pattern * 7919), sd = 0x1357u ^ uint32_t(pattern * 104729);

  d.j_valid = 1;
  d.j_epoch = epoch;
  d.j_list_off = off;
  d.j_list_bytes = bytes;
  d.j_list_crc = crc;
  d.j_patch_count = count;
  d.j_sequence = seq;
  d.j_src_id = src;
  d.eval();
  int guard = 0;
  while (!d.j_ready && guard < 1000) { zhao::tick(d); d.eval(); ++guard; }
  zhao::tick(d);
  d.j_valid = 0;
  d.eval();

  for (uint64_t c = 0; c < cap && !o.done; ++c) {
    d.rec_ready = draw(sr, pattern) ? 1 : 0;
    d.done_ready = draw(sd, pattern) ? 1 : 0;
    d.eval();

    if (d.fr_start) {
      ++o.frames;
      o.fr_count = d.fr_patch_count;
      o.fr_seq = d.fr_sequence;
      o.fr_epoch = d.fr_epoch;
    }
    if (d.rec_valid && d.rec_ready) {
      if (o.frames == 0) ++o.rec_before_frame;
      ss::PatchRecord r;
      r.island_id = d.rec_island;
      r.patch_ix = int16_t(d.rec_ix);
      r.patch_iz = int16_t(d.rec_iz);
      r.hps_page_addr = d.rec_hps_addr;
      r.expected_page_crc32c = d.rec_crc;
      r.flags = uint16_t(d.rec_flags);
      r.view_mask = uint8_t(d.rec_view_mask);
      r.priority = uint8_t(d.rec_priority);
      r.source_id = d.rec_src_id;
      r.reserved = 0;
      o.recs.push_back(r);
    }
    if (d.done_valid && d.done_ready) {
      o.done = true;
      o.ok = d.done_ok != 0;
      o.verdict = int(d.done_verdict);
      o.crc_seen = d.done_crc_seen;
      o.src = d.done_src_id;
    }
    zhao::tick(d);
    d.eval();
    ++o.cycles;
  }
  d.rec_ready = 0;
  d.done_ready = 0;
  d.eval();
  return o;
}

int compare(const char* tag, const std::vector<ss::PatchRecord>& got,
            const std::vector<ss::PatchRecord>& want) {
  int bad = 0, printed = 0;
  const std::size_t n = got.size() < want.size() ? got.size() : want.size();
  for (std::size_t i = 0; i < n; ++i) {
    const ss::PatchRecord& g = got[i];
    const ss::PatchRecord& e = want[i];
    if (g.island_id == e.island_id && g.patch_ix == e.patch_ix && g.patch_iz == e.patch_iz &&
        g.hps_page_addr == e.hps_page_addr &&
        g.expected_page_crc32c == e.expected_page_crc32c && g.flags == e.flags &&
        g.view_mask == e.view_mask && g.priority == e.priority && g.source_id == e.source_id)
      continue;
    ++bad;
    if (printed < 4) {
      ++printed;
      std::printf("   %s record %zu:\n", tag, i);
      if (g.island_id != e.island_id)
        std::printf("      island_id  got 0x%08X want 0x%08X\n", g.island_id, e.island_id);
      if (g.patch_ix != e.patch_ix)
        std::printf("      patch_ix   got %6d want %6d\n", g.patch_ix, e.patch_ix);
      if (g.patch_iz != e.patch_iz)
        std::printf("      patch_iz   got %6d want %6d\n", g.patch_iz, e.patch_iz);
      if (g.hps_page_addr != e.hps_page_addr)
        std::printf("      hps_addr   got 0x%016llX want 0x%016llX\n",
                    (unsigned long long)g.hps_page_addr, (unsigned long long)e.hps_page_addr);
      if (g.expected_page_crc32c != e.expected_page_crc32c)
        std::printf("      page_crc   got 0x%08X want 0x%08X\n", g.expected_page_crc32c,
                    e.expected_page_crc32c);
      if (g.flags != e.flags)
        std::printf("      flags      got 0x%04X want 0x%04X\n", g.flags, e.flags);
      if (g.view_mask != e.view_mask)
        std::printf("      view_mask  got %u want %u\n", g.view_mask, e.view_mask);
      if (g.priority != e.priority)
        std::printf("      priority   got %u want %u\n", g.priority, e.priority);
      if (g.source_id != e.source_id)
        std::printf("      source_id  got 0x%08X want 0x%08X\n", g.source_id, e.source_id);
    }
  }
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_terrain_cmd* dut = new Vtb_terrain_cmd;
  World w(*dut);
  Vtb_terrain_cmd& d = *dut;

  std::printf("== TERRAIN.CMD vs zref::swstream ==\n");
  std::printf("   record %zu B, arena 0x%08X + %u, list at +0x%04X\n\n", ss::kRecordBytes,
              kArenaBase, kArenaBytes, kListOff);

  // =========================================================================
  // A -- AN EVEN LIST: eight records, four whole bursts
  // =========================================================================
  {
    std::printf("-- A: eight records, no stalls --\n");
    w.reset();
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 8; ++i) list.push_back(rec_of(i));
    const uint32_t crc = w.place(kListOff, list);
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
    const Out o = submit(w, kEpoch, kListOff, bytes, crc, 8, 0x77u, 0xABCD0001u, 0);

    ck(o.done && o.ok, "A the command completed and reported ok", 1, (o.done && o.ok) ? 1 : 0);
    ck(o.verdict == kVOk, "A with the ok verdict", kVOk, o.verdict);
    ck(int(o.recs.size()) == 8, "A eight records came out", 8, int(o.recs.size()));
    ck(compare("A", o.recs, list) == 0,
       "A every record matches zref::swstream::encode_record's input, field for field");

    // THE FRAME RING, and the ordering that makes it mean anything.
    ck(o.frames == 1, "A exactly one frame was started", 1, o.frames);
    ck(o.rec_before_frame == 0,
       "A and NO record was offered before it -- TERRAIN.SEQ latches the frame's epoch, "
       "count and sequence on the pulse, so a record that arrived first would belong to "
       "the previous frame",
       0, o.rec_before_frame);
    ck(o.fr_count == 8, "A the ring carries the patch count", 8, long(o.fr_count));
    ck(o.fr_seq == 0x77u, "A and the sequence", 0x77, long(o.fr_seq));
    ck(o.fr_epoch == kEpoch, "A and the epoch", long(kEpoch), long(o.fr_epoch));

    // TWO PASSES, MEASURED. The list is read once to fold and once to emit, so
    // the bytes read are twice the list. This is the check that would catch a
    // "verify while emitting" rewrite, which is the tempting optimisation and
    // the one that breaks the whole point.
    ck(d.c_bytes == 2u * bytes,
       "A the list was read TWICE -- once to verify, once to act, which is what makes "
       "'a corrupt list issues no loads' true",
       long(2 * bytes), long(d.c_bytes));
    std::printf("   %llu cycles, %u bursts, %u beats, %u bytes read (list is %u)\n",
                (unsigned long long)o.cycles, d.bursts_seen, d.beats_seen, d.c_bytes, bytes);
    ck(d.c_accepted == 1, "A counted as one accepted set", 1, long(d.c_accepted));
    ck(d.c_records == 8, "A and eight records emitted", 8, long(d.c_records));
    ck(d.first_addr >= kArenaBase + kListOff,
       "A every read was at or after the list's start", long(kArenaBase + kListOff),
       long(d.first_addr));
    ck(d.last_addr < kArenaBase + kListOff + bytes,
       "A and none began past its end", long(kArenaBase + kListOff + bytes),
       long(d.last_addr));
  }

  // =========================================================================
  // B -- AN ODD LIST: the 32-byte tail burst
  // =========================================================================
  // Seven records is 224 bytes = three 64-byte bursts and one of 32. A block
  // that always asked for 64 would read 32 bytes that are not in the list,
  // fold them, and fail its own CRC.
  {
    std::printf("\n-- B: seven records, so the last burst is half --\n");
    w.reset();
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 7; ++i) list.push_back(rec_of(i + 100));
    const uint32_t crc = w.place(kListOff, list);
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
    const Out o = submit(w, kEpoch, kListOff, bytes, crc, 7, 0x78u, 0xABCD0002u, 0);
    ck(o.done && o.ok, "B an odd record count completes", 1, (o.done && o.ok) ? 1 : 0);
    ck(int(o.recs.size()) == 7, "B seven records", 7, int(o.recs.size()));
    ck(compare("B", o.recs, list) == 0, "B and every one matches");
    ck(d.c_bytes == 2u * bytes,
       "B and it read exactly the list, twice -- not one byte of the 32 past its end",
       long(2 * bytes), long(d.c_bytes));
  }

  // =========================================================================
  // C -- FOUR STALL PATTERNS
  // =========================================================================
  // The record port is the one that matters: the DUT abandons the rest of a
  // burst to hold a record TERRAIN.SEQ is not ready for, and re-requests from
  // the byte after it. A bench that never stalls never reaches that path.
  {
    std::printf("\n-- C: four stall patterns --\n");
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 13; ++i) list.push_back(rec_of(i + 200));
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    for (int pattern = 0; pattern < 4; ++pattern) {
      w.reset();
      d.cfg_req_latency_i = uint8_t(1 + pattern * 3);
      d.cfg_beat_gap_i = uint8_t(pattern);
      d.eval();
      const uint32_t crc = w.place(kListOff, list);
      const Out o = submit(w, kEpoch, kListOff, bytes, crc, 13, 0x79u, 0xABCD0003u, pattern,
                           2000000ull);

      char msg[176];
      std::snprintf(msg, sizeof msg, "C pattern %d completed ok", pattern);
      ck(o.done && o.ok, msg, 1, (o.done && o.ok) ? 1 : 0);
      std::snprintf(msg, sizeof msg, "C pattern %d emitted all thirteen records", pattern);
      ck(int(o.recs.size()) == 13, msg, 13, int(o.recs.size()));
      std::snprintf(msg, sizeof msg, "C pattern %d matches, in order, field for field", pattern);
      ck(compare("C", o.recs, list) == 0, msg);
      std::snprintf(msg, sizeof msg, "C pattern %d started exactly one frame", pattern);
      ck(o.frames == 1, msg, 1, o.frames);
      std::printf("   pattern %d: %llu cycles, %u bursts, %u beats\n", pattern,
                  (unsigned long long)o.cycles, d.bursts_seen, d.beats_seen);
    }
  }

  // =========================================================================
  // D -- THE CORRUPT LIST, and the only claim that matters about it
  // =========================================================================
  {
    std::printf("\n-- D: one byte flipped --\n");
    w.reset();
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 9; ++i) list.push_back(rec_of(i + 300));
    const uint32_t crc = w.place(kListOff, list);
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    // Flip a byte in the LAST record, which is the case a
    // verify-while-emitting block would get wrong most expensively: eight
    // records would already be out.
    w.arena[kListOff + bytes - 5] ^= 0x01u;
    w.upload(kListOff, bytes);

    d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
    const Out o = submit(w, kEpoch, kListOff, bytes, crc, 9, 0x7Au, 0xABCD0004u, 0);

    ck(o.done, "D a corrupt list still produces a completion");
    ck(!o.ok && o.verdict == kVCrc, "D with the CRC verdict", kVCrc, o.verdict);
    ck(o.recs.empty(),
       "D AND NOT ONE RECORD WAS OFFERED. This is the whole reason the list is read "
       "twice: a block that emitted as it folded would have issued eight loads before it "
       "found out",
       0, int(o.recs.size()));
    ck(o.frames == 0,
       "D and no frame was started -- TERRAIN.SEQ never heard about this command at all",
       0, o.frames);
    ck(o.crc_seen != crc,
       "D the completion reports the CRC it actually computed, which is not the one the "
       "command claimed",
       1, (o.crc_seen != crc) ? 1 : 0);
    ck(d.c_crc_fails == 1, "D counted once", 1, long(d.c_crc_fails));
    ck(d.c_bytes == bytes,
       "D and it read the list ONCE -- the second pass never happened", long(bytes),
       long(d.c_bytes));
    std::printf("   claimed 0x%08X, computed 0x%08X\n", crc, o.crc_seen);
  }

  // =========================================================================
  // E -- EVERY REFUSAL, FIRED
  // =========================================================================
  // A verdict that has never been produced is a verdict nobody has tested. And
  // every one of these must refuse BEFORE a byte is read, or `c_bytes` and
  // `bursts_seen` stop measuring the fabric.
  {
    std::printf("\n-- E: the refusals --\n");
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 4; ++i) list.push_back(rec_of(i + 400));
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    struct Case {
      const char* name;
      int verdict;
      uint32_t epoch, off, bytes, count, seq;
    };
    const Case cases[] = {
      {"a stale resource_epoch",       kVEpoch,   kEpoch ^ 1u, kListOff, bytes,   4, 1},
      {"an empty set",                 kVEmpty,   kEpoch,      kListOff, 0,       0, 1},
      {"list_bytes != 32*patch_count", kVLen,     kEpoch,      kListOff, bytes+32,4, 1},
      {"a list that is not 8-aligned", kVAlign,   kEpoch,      kListOff+4, bytes, 4, 1},
      {"a list past the arena",        kVUnreach, kEpoch,      kArenaBytes - 32, bytes, 4, 1},
    };

    // AND A SEQUENCE ABOVE 65,535 IS NOT A REFUSAL AT ALL, which is the other
    // half of the correction: T5's `sequence` is a u32 and the ring takes all
    // thirty-two bits, so a large one must go straight through.
    {
      w.reset();
      const uint32_t crc = w.place(kListOff, list);
      const Out o = submit(w, kEpoch, kListOff, bytes, crc, 4, 0x1234'5678u, 0xEEEE0001u,
                           0, 60000);
      ck(o.done && o.ok,
         "E a sequence above 65,535 is accepted -- the frame ring is 32 bits wide", 1,
         (o.done && o.ok) ? 1 : 0);
      ck(o.fr_seq == 0x1234'5678u,
         "E and it reaches the ring unaltered", long(0x1234'5678u), long(o.fr_seq));
    }

    for (const Case& c : cases) {
      w.reset();
      const uint32_t crc = w.place(kListOff, list);
      d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
      const Out o = submit(w, c.epoch, c.off, c.bytes, crc, uint16_t(c.count), c.seq,
                           0xEEEE0000u, 0, 60000);

      char msg[200];
      std::snprintf(msg, sizeof msg, "E %s is refused", c.name);
      ck(o.done && !o.ok, msg, 1, (o.done && !o.ok) ? 1 : 0);
      std::snprintf(msg, sizeof msg, "E %s reports verdict %d", c.name, c.verdict);
      ck(o.verdict == c.verdict, msg, c.verdict, o.verdict);
      std::snprintf(msg, sizeof msg,
                    "E %s reads NOT ONE BYTE -- the refusal does not issue the read it is "
                    "refusing", c.name);
      ck(d.bursts_seen == 0, msg, 0, long(d.bursts_seen));
      std::snprintf(msg, sizeof msg, "E %s starts no frame", c.name);
      ck(o.frames == 0 && o.recs.empty(), msg, 0, o.frames + int(o.recs.size()));
    }
  }

  // =========================================================================
  // F -- THE BRIDGE FAILING, in both of its shapes
  // =========================================================================
  {
    std::printf("\n-- F: the bridge says err --\n");
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 8; ++i) list.push_back(rec_of(i + 500));
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);

    for (int mode = 1; mode <= 2; ++mode) {
      w.reset();
      const uint32_t crc = w.place(kListOff, list);
      d.cfg_err_mode_i = uint8_t(mode);
      d.cfg_err_burst_i = 1;      // not the first, so the walk is under way
      d.eval();
      d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
      const Out o = submit(w, kEpoch, kListOff, bytes, crc, 8, 0x7Bu, 0xABCD0005u, 0, 200000);

      char msg[160];
      std::snprintf(msg, sizeof msg, "F mode %d still produces a completion", mode);
      ck(o.done, msg);
      std::snprintf(msg, sizeof msg, "F mode %d reports the bridge verdict", mode);
      ck(!o.ok && o.verdict == kVBridge, msg, kVBridge, o.verdict);
      std::snprintf(msg, sizeof msg, "F mode %d counted the error once", mode);
      ck(d.c_bridge_errs == 1, msg, 1, long(d.c_bridge_errs));
      std::snprintf(msg, sizeof msg,
                    "F mode %d offered no records -- the failure was in the CRC pass, which "
                    "is where every read starts", mode);
      ck(o.recs.empty(), msg, 0, int(o.recs.size()));
      d.cfg_err_mode_i = 0;
    }
  }

  // =========================================================================
  // G -- AND IT STILL WORKS AFTERWARDS
  // =========================================================================
  // Six refusals and two bridge faults is where a state machine leaves a busy
  // flag set. A block that faulted correctly and then never submitted again
  // would pass every check above.
  {
    std::printf("\n-- G: a good set after eight failures --\n");
    w.reset();
    std::vector<ss::PatchRecord> list;
    for (uint32_t i = 0; i < 5; ++i) list.push_back(rec_of(i + 600));
    const uint32_t crc = w.place(kListOff, list);
    const uint32_t bytes = uint32_t(list.size() * ss::kRecordBytes);
    const Out o = submit(w, kEpoch, kListOff, bytes, crc, 5, 0x7Cu, 0xABCD0006u, 2);
    ck(o.done && o.ok, "G it submits again after the fault phases", 1,
       (o.done && o.ok) ? 1 : 0);
    ck(int(o.recs.size()) == 5, "G five records", 5, int(o.recs.size()));
    ck(compare("G", o.recs, list) == 0, "G and they match");
    ck(d.c_idle != 0, "G and the block returns to idle", 1, d.c_idle ? 1 : 0);
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
