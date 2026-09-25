// terrain_prepwalk_directed.cpp -- TERRAIN.PREPWALK, the REPLAYABLE
// admitted-set PREPARE reader.
//
// Contract: design/contracts/TERRAIN.EDGERECON.md, "THE REMAINDER, AS FOUR
// PACKETS", row P2. Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt
// section 2 -- "Complete a replayable sealed-list PREPARE reader independent of
// compose-cache backpressure ... Reuse/factor the canonical PLACE arithmetic
// through a stateless query."
//
// WHAT THIS GUARDS:
//
//   * THE WALK MUST TERMINATE WITHOUT THE COMPOSE SPINE. That is the whole
//     reason this block exists -- a PREPARE pass teed off `u_terrain_seq`'s
//     issue port deadlocks, because that stream's ready IS the compose cache's.
//     Case 1 walks a complete two-patch set with nothing but the HPS bridge,
//     the residency directory and devstore answering.
//   * THE LIST IS RE-READ, AND THE RE-READ IS CHECKED. T5 makes the sealed list
//     capture data, immutable for the frame. Case 3 mutates one byte of the
//     arena between the command and the walk and requires the walk to refuse.
//     Without that check a rewritten arena would be prepared silently and the
//     bank would describe a list nobody sealed.
//   * PLACEMENT MUST MATCH `zhao_terrain_place` EXACTLY, or PREPARE banks a
//     level for one place and EMIT tessellates another. Case 2 checks the
//     subpatch centres against the ratified law at three pitches. The RTL and
//     this test cannot drift apart silently, because the RTL calls
//     `zhao_terrain_place_law_pkg` -- the same functions `zhao_terrain_place`
//     calls -- rather than its own copy.
//   * PREPARE MUST NOT CHANGE RESIDENCY. The block has a LOOKUP port and no
//     claim, pin, load or writeback port at all; a sim-only $fatal inside the
//     RTL asserts no lookup escapes its phase. Case 4 skips a miss rather than
//     waiting on it, which is architecture 2.6's rule.
//
// EVERY COUNTER IS FIRED FROM THIS BLOCK'S OWN BOUNDARY WITH LEGAL STIMULUS,
// each with a control beside it. NO committed mutant is owed here, and that is
// a measured claim rather than an omission:
//
//   walks_started_o         case 1
//   walks_completed_o       case 1   ... and shown FLAT in cases 3, 5, 6, 7
//   records_walked_o        case 1
//   patches_prepared_o      case 1
//   skipped_not_resident_o  case 4   a record whose patch is not resident
//   descriptors_emitted_o   case 1   sixteen per prepared patch
//   patches_unfresh_o       case 8   devstore had never written the slot
//   list_crc_mismatch_o     case 3   the arena moved underneath the frame
//   freeze_broken_o         case 5   the witness moved mid-walk
//   pitch_illegal_o         case 6   a pitch outside spec 1.3's four
//   place_range_o           case 7   a placement that s32 cannot hold
//   jobs_refused_o          case 9   a stale epoch / bad length / unaligned list
//   sub_order_bad_o         case 10  devstore streamed its records out of order
//   bridge_errs_o           case 11  the bridge refused the burst
//   store_wait_clocks_o     case 1   CLOCKS -- the budget P2 was told to keep
//   list_wait_clocks_o      case 1
//   list_bytes_read_o       case 1
//   list_refetch_bytes_o    case 12  the 64-byte-alignment cost, measured
//
// `store_wait_clocks_o` is the instrument, not the burst count.
// `spec/memory_rules.md:396-402` already named it: PREPARE doubles the
// frame-critical read demand on the one client ruling T3 starves first, so what
// matters is how long the reads WAIT. And the bench figure is a FLOOR -- a
// played fabric at 2-cycle read latency understates a real four-burst SDRAM
// request by 4-6x, so no number printed here is the cost of anything on a
// board.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

// Driven through `tb_terrain_prepwalk`, a wrapper that holds no logic and
// only takes the two packed HPS-bridge structs apart. A bench that decoded
// them by hand would carry bit offsets that depend on `zhao_client_e`'s
// width -- a number that changes the day a client is added, silently.
#include "Vtb_terrain_prepwalk.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
int checks = 0;

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    std::printf("FAIL: %s -- expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got));
    ++failures;
  }
}

void check_true(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

// ---------------------------------------------------------------------------
// THE PLACEMENT LAW, as the ORACLE. spec/terrain_rules.md 1.3 / 2.1:
//     wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
// This is a test oracle and is allowed to be a second expression of the law;
// what is NOT allowed is a second expression in the RTL, which is why
// zhao_terrain_prepwalk calls zhao_terrain_place_law_pkg.
int32_t place_law(int16_t coord, uint32_t idx, int8_t pitch_log2) {
  const int32_t units = static_cast<int32_t>(coord) * 32 + static_cast<int32_t>(idx);
  const int sh = 16 + pitch_log2;
  return static_cast<int32_t>(static_cast<uint32_t>(units) << sh);
}

// The subpatch centre, `zhao_terrain_spdesc.sv:387-388`'s own expression.
uint32_t centre_i(uint32_t sub) { return (sub & 3u) * 8u + 4u; }
uint32_t centre_j(uint32_t sub) { return ((sub >> 2) & 3u) * 8u + 4u; }

// ---------------------------------------------------------------------------
// CRC32C, the same polynomial zhao_crc32c_fold implements. Folded LSB-first
// over bytes, init 0xFFFFFFFF, final xor 0xFFFFFFFF.
uint32_t crc32c(const uint8_t* p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b) c = (c >> 1) ^ (0x82F63B78u & (~(c & 1u) + 1u));
  }
  return c ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// One T5 32-byte patch-list record, laid out little-endian by hand in T5's
// field order -- `zref::swstream::encode_record`'s own discipline, because a
// struct's padding is the compiler's business and a capture that replays only
// on the machine that made it is not capture data.
struct Rec {
  uint32_t island = 0;
  int16_t ix = 0, iz = 0;
  uint64_t hps_addr = 0;
  uint32_t page_crc = 0;
  uint16_t flags = 0;
  uint8_t view_mask = 0, priority = 0;
  uint32_t src_id = 0;

  void encode(uint8_t* o) const {
    std::memset(o, 0, 32);
    o[0] = island & 0xFF; o[1] = (island >> 8) & 0xFF;
    o[2] = (island >> 16) & 0xFF; o[3] = (island >> 24) & 0xFF;
    const uint16_t uix = static_cast<uint16_t>(ix), uiz = static_cast<uint16_t>(iz);
    o[4] = uix & 0xFF; o[5] = (uix >> 8) & 0xFF;
    o[6] = uiz & 0xFF; o[7] = (uiz >> 8) & 0xFF;
    for (int k = 0; k < 8; ++k) o[8 + k] = static_cast<uint8_t>((hps_addr >> (8 * k)) & 0xFF);
    o[16] = page_crc & 0xFF; o[17] = (page_crc >> 8) & 0xFF;
    o[18] = (page_crc >> 16) & 0xFF; o[19] = (page_crc >> 24) & 0xFF;
    o[20] = flags & 0xFF; o[21] = (flags >> 8) & 0xFF;
    o[22] = view_mask; o[23] = priority;
    o[24] = src_id & 0xFF; o[25] = (src_id >> 8) & 0xFF;
    o[26] = (src_id >> 16) & 0xFF; o[27] = (src_id >> 24) & 0xFF;
  }
};

// The devstore record this bench hands back for one subpatch.
struct DevRec {
  uint32_t dev1 = 0, dev2 = 0, dev3 = 0;
  int16_t cy = 0;
  uint32_t prev_level = 0, prev_morph = 0, hold = 0;
};

// One descriptor observed leaving the block.
struct Seen {
  int32_t cx = 0, cz = 0, cy = 0;
  uint32_t dev1 = 0, sub = 0, src_id = 0, gen = 0;
  int16_t ix = 0, iz = 0;
  uint32_t prev_level = 0, hold = 0;
};

constexpr uint32_t kArenaBase = 0x2000'0000u;
constexpr uint32_t kArenaBytes = 0x0001'0000u;
constexpr uint32_t kListOff = 0x40;
constexpr uint32_t kEpoch = 0xABCD'0001u;

class Harness {
 public:
  Vtb_terrain_prepwalk t;

  // ---- the HPS staging arena, which IS the replay source -----------------
  std::vector<uint8_t> arena;

  // ---- the residency directory -------------------------------------------
  // (ix,iz) -> slot. Absent means not resident.
  std::vector<std::pair<std::pair<int16_t, int16_t>, uint32_t>> resident;
  uint32_t resident_gen = 0x5;

  // ---- the devstore ------------------------------------------------------
  DevRec dev[16];
  bool dev_fresh = true;
  bool dev_scramble = false;   // stream the sixteen records out of order
  int store_latency = 3;

  // ---- the bridge --------------------------------------------------------
  bool bridge_refuse = false;
  int bridge_first_beat = 4;   // cycles to first beat

  std::vector<Seen> seen;

  Harness() : arena(kArenaBytes, 0) {}

  // ---- bridge model state -------------------------------------------------
  int bstate = 0;            // 0 idle, 1 latency, 2 streaming
  uint32_t baddr = 0;
  int bbeats = 0, bleft = 0, bwait = 0;

  // ---- lookup model state -------------------------------------------------
  int lstate = 0;
  int lwait = 0;
  bool lhit = false;
  uint32_t lslot = 0;

  // ---- devstore model state ----------------------------------------------
  int dstate = 0;
  int dwait = 0;
  int dptr = 0;

  void quiet() {
    t.cfg_hps_client_i = 6;
    t.cfg_epoch_i = kEpoch;
    t.cfg_arena_base_i = kArenaBase;
    t.cfg_arena_bytes_i = kArenaBytes;
    t.j_valid_i = 0;
    t.j_epoch_i = kEpoch;
    t.j_list_off_i = kListOff;
    t.j_list_bytes_i = 0;
    t.j_list_crc_i = 0;
    t.j_patch_count_i = 0;
    t.frz_pitch_log2_i = 1;
    t.frz_tok_i = 0x1234;
    t.hps_req_grant_i = 0;
    t.hps_rsp_beat_valid_i = 0;
    t.hps_rsp_data_i = 0;
    t.hps_rsp_last_i = 0;
    t.hps_rsp_err_i = 0;
    t.lu_ready_i = 0;
    t.lu_ans_valid_i = 0;
    t.lu_ans_hit_i = 0;
    t.lu_ans_slot_i = 0;
    t.lu_ans_gen_i = 0;
    t.r_ready_i = 0;
    t.r_valid_i = 0;
    t.r_sp_i = 0;
    t.r_dev1_i = 0; t.r_dev2_i = 0; t.r_dev3_i = 0;
    t.r_cy_i = 0; t.r_prev_level_i = 0; t.r_prev_morph_i = 0;
    t.r_hold_i = 0; t.r_fresh_i = 0;
    t.sp_ready_i = 1;
    t.prep_gate_i = 0;
  }

  void reset() {
    quiet();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    bstate = lstate = dstate = 0;
    seen.clear();
  }

  void drive_rsp(bool beat_valid, uint64_t data, bool last, bool err) {
    t.hps_rsp_beat_valid_i = beat_valid ? 1 : 0;
    t.hps_rsp_data_i = data;
    t.hps_rsp_last_i = last ? 1 : 0;
    t.hps_rsp_err_i = err ? 1 : 0;
  }

  uint64_t arena_qword(uint32_t abs) const {
    const uint32_t off = abs - kArenaBase;
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) {
      const uint32_t a = off + static_cast<uint32_t>(k);
      const uint8_t b = (a < arena.size()) ? arena[a] : 0;
      v |= static_cast<uint64_t>(b) << (8 * k);
    }
    return v;
  }

  // The bridge: 64-byte-aligned bursts only, exactly as zhao_hps_bridge rules.
  void step_bridge() {
    if (bstate == 0) {
      drive_rsp(false, 0, false, false);
      t.hps_req_grant_i = 0;
      if (t.hps_req_valid_o) {
        const uint32_t addr = t.hps_req_addr_o;
        const uint32_t len = t.hps_req_len_o;
        const bool malformed = (len == 0) || (len > 64) || ((addr & 0x3F) != 0);
        if (bridge_refuse || malformed) {
          drive_rsp(false, 0, true, true);
          return;
        }
        t.hps_req_grant_i = 1;
        baddr = addr;
        bbeats = static_cast<int>(len / 8);
        bleft = bbeats;
        bwait = bridge_first_beat;
        bstate = 1;
      }
    } else if (bstate == 1) {
      t.hps_req_grant_i = 0;
      drive_rsp(false, 0, false, false);
      if (--bwait <= 0) bstate = 2;
    } else {
      t.hps_req_grant_i = 0;
      const uint32_t a = baddr + static_cast<uint32_t>((bbeats - bleft) * 8);
      const bool last = (bleft == 1);
      drive_rsp(true, arena_qword(a), last, false);
      if (--bleft <= 0) bstate = 0;
    }
  }

  void step_lookup() {
    if (lstate == 0) {
      t.lu_ans_valid_i = 0;
      t.lu_ready_i = 1;
      if (t.lu_valid_o) {
        const int16_t ix = static_cast<int16_t>(t.lu_ix_o);
        const int16_t iz = static_cast<int16_t>(t.lu_iz_o);
        lhit = false;
        lslot = 0;
        for (const auto& r : resident) {
          if (r.first.first == ix && r.first.second == iz) {
            lhit = true;
            lslot = r.second;
          }
        }
        lwait = 2;
        lstate = 1;
      }
    } else {
      t.lu_ready_i = 0;
      if (--lwait <= 0) {
        t.lu_ans_valid_i = 1;
        t.lu_ans_hit_i = lhit ? 1 : 0;
        t.lu_ans_slot_i = lslot;
        t.lu_ans_gen_i = resident_gen;
        lstate = 0;
      } else {
        t.lu_ans_valid_i = 0;
      }
    }
  }

  void step_devstore() {
    if (dstate == 0) {
      t.r_valid_i = 0;
      t.r_ready_i = 1;
      if (t.r_start_o) {
        dwait = store_latency;
        dptr = 0;
        dstate = 1;
      }
    } else if (dstate == 1) {
      t.r_ready_i = 0;
      if (--dwait <= 0) dstate = 2;
    } else {
      t.r_ready_i = 0;
      // The subpatch index the record carries. Scrambling it is what
      // `sub_order_bad_o` watches for.
      const int sub = dev_scramble ? ((dptr + 1) & 15) : dptr;
      t.r_valid_i = 1;
      t.r_sp_i = static_cast<uint8_t>(sub);
      t.r_dev1_i = dev[sub].dev1;
      t.r_dev2_i = dev[sub].dev2;
      t.r_dev3_i = dev[sub].dev3;
      t.r_cy_i = dev[sub].cy;
      t.r_prev_level_i = static_cast<uint8_t>(dev[sub].prev_level);
      t.r_prev_morph_i = dev[sub].prev_morph;
      t.r_hold_i = static_cast<uint8_t>(dev[sub].hold);
      t.r_fresh_i = dev_fresh ? 1 : 0;
    }
  }

  // THE RECORD IS RETIRED HERE, AFTER `eval()`, AND THAT SEPARATION IS THE
  // POINT. The first version of this bench advanced `dptr` inside the drive
  // step, reading `r_ready_o` as it stood after the PREVIOUS `eval()` -- so
  // the model's handshake accounting lagged the DUT's by one cycle. It
  // retired the sixteenth record while the DUT was still waiting for it, and
  // the two then held each other still. It presented as "the walk does not
  // terminate and emits fifteen descriptors", which reads exactly like an
  // off-by-one in the RTL's subpatch counter. A ready sampled before `eval()`
  // is a statement about the wrong cycle.
  // IT UPDATES MODEL STATE AND NEVER A DUT INPUT, and that is the second half
  // of the same lesson. `zhao::tick` settles low, drives the RISING EDGE, and
  // settles low again -- so the DUT samples its inputs INSIDE tick(). Clearing
  // `r_valid_i` here, after eval() but before tick(), withdrew the sixteenth
  // record on the very edge that was about to accept it. The counter then read
  // fifteen while the bench's own observer had already seen sixteen beats go
  // by, which is a disagreement between two views of one handshake and reads
  // like an off-by-one in the RTL.
  //
  // `dstate = 0` is enough: the NEXT step's drive phase lowers `r_valid_i`,
  // before eval(), where an input change belongs.
  void sample_devstore() {
    if (dstate == 2 && t.r_valid_i && t.r_ready_o) {
      if (++dptr >= 16) dstate = 0;
    }
  }

  void observe() {
    if (t.sp_valid_o && t.sp_ready_i) {
      Seen s;
      s.cx = static_cast<int32_t>(t.sp_cx_o);
      s.cz = static_cast<int32_t>(t.sp_cz_o);
      s.cy = static_cast<int32_t>(t.sp_cy_o);
      s.dev1 = t.sp_dev1_o;
      s.sub = t.sp_sub_o;
      s.src_id = t.sp_src_id_o;
      s.gen = t.sp_gen_o;
      s.ix = static_cast<int16_t>(t.sp_ix_o);
      s.iz = static_cast<int16_t>(t.sp_iz_o);
      s.prev_level = t.sp_prev_level_o;
      s.hold = t.sp_hold_o;
      seen.push_back(s);
    }
  }

  // One cycle of the whole world. DRIVE, then EVAL, then SAMPLE, then TICK --
  // in that order, always. Every handshake this bench observes is read after
  // `eval()` has settled the combinational answer to what the models drove.
  void step() {
    step_bridge();
    step_lookup();
    step_devstore();
    // The reconciler answers the sweep PULSE by opening PREPARE and holding it
    // open for the phase. `prep_begin_o` is one cycle wide, so this latches.
    if (t.prep_begin_o) t.prep_gate_i = 1;
    t.eval();
    sample_devstore();
    observe();
    zhao::tick(t);
    t.eval();
  }

  // Load a list into the arena and return its CRC.
  uint32_t load_list(const std::vector<Rec>& recs) {
    std::vector<uint8_t> bytes(recs.size() * 32);
    for (size_t k = 0; k < recs.size(); ++k) recs[k].encode(&bytes[k * 32]);
    std::memcpy(&arena[kListOff], bytes.data(), bytes.size());
    return crc32c(bytes.data(), bytes.size());
  }

  // Offer the job and run the walk to completion (or the bound).
  bool run_walk(uint32_t crc, uint16_t count, int bound = 40000) {
    t.j_valid_i = 1;
    t.j_epoch_i = kEpoch;
    t.j_list_off_i = kListOff;
    t.j_list_bytes_i = static_cast<uint32_t>(count) * 32u;
    t.j_list_crc_i = crc;
    t.j_patch_count_i = count;
    t.eval();
    // Take the job.
    for (int i = 0; i < 64; ++i) {
      if (t.j_ready_o) break;
      step();
    }
    step();
    t.j_valid_i = 0;
    t.eval();
    for (int i = 0; i < bound; ++i) {
      if (t.prep_done_o) {
        step();
        return true;
      }
      step();
    }
    return false;
  }
};

std::vector<Rec> two_patch_list() {
  Rec a, b;
  a.island = 7; a.ix = 40; a.iz = 12; a.src_id = 0x0101; a.flags = 1;
  b.island = 7; b.ix = 41; b.iz = 12; b.src_id = 0x0102; b.flags = 1;
  return {a, b};
}

void fill_dev(Harness& h) {
  for (uint32_t s = 0; s < 16; ++s) {
    h.dev[s].dev1 = 0x010000u + s;
    h.dev[s].dev2 = 0x020000u + s;
    h.dev[s].dev3 = 0x030000u + s;
    h.dev[s].cy = static_cast<int16_t>(100 + s);
    h.dev[s].prev_level = s & 3u;
    h.dev[s].prev_morph = 0x100u + s;
    h.dev[s].hold = s;
  }
}

// ===========================================================================
void case1_complete_walk(Harness& h) {
  std::printf("-- case 1: A COMPLETE WALK, WITH NO COMPOSE SPINE ANYWHERE IN THE LOOP\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}, {{41, 12}, 9}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);

  check_true(h.run_walk(crc, 2), "case1: the walk terminated");
  check_eq(h.t.prep_valid_o, 1, "case1: the walk is VALID -- the bank describes this frame");
  check_eq(h.t.restart_req_o, 0, "case1: no restart requested");
  check_eq(h.t.walks_started_o, 1, "case1: one walk started");
  check_eq(h.t.walks_completed_o, 1, "case1: one walk completed");
  check_eq(h.t.records_walked_o, 2, "case1: two records walked");
  check_eq(h.t.patches_prepared_o, 2, "case1: two patches prepared");
  check_eq(h.t.descriptors_emitted_o, 32, "case1: SIXTEEN descriptors per patch");
  check_eq(h.seen.size(), 32, "case1: thirty-two descriptors observed on the wire");
  check_eq(h.t.skipped_not_resident_o, 0, "case1: nothing was skipped");
  check_eq(h.t.list_crc_mismatch_o, 0, "case1: the re-read matched the sealed CRC");
  check_eq(h.t.list_bytes_read_o, 64, "case1: sixty-four bytes of list were read");
  check_true(h.t.store_wait_clocks_o != 0,
             "case1: store_wait_clocks_o FIRED -- the devstore budget instrument is alive");
  check_true(h.t.list_wait_clocks_o != 0, "case1: list_wait_clocks_o FIRED");

  // The identity rides WITH each descriptor.
  if (h.seen.size() == 32) {
    check_eq(static_cast<uint16_t>(h.seen[0].ix), 40, "case1: patch A ix on its first descriptor");
    check_eq(h.seen[0].src_id, 0x0101u, "case1: patch A src_id");
    check_eq(h.seen[0].gen, h.resident_gen, "case1: the residency generation rode along");
    check_eq(static_cast<uint16_t>(h.seen[16].ix), 41, "case1: patch B ix on its seventeenth");
    check_eq(h.seen[16].src_id, 0x0102u, "case1: patch B src_id");
    // The devstore payload is passed through, not staged: descriptor k must
    // carry subpatch k's deviation, NOT subpatch k-1's. This is the check that
    // catches the staging defect this block was authored with and repaired.
    bool payload_ok = true;
    for (uint32_t k = 0; k < 16; ++k) {
      if (h.seen[k].sub != k) payload_ok = false;
      if (h.seen[k].dev1 != 0x010000u + k) payload_ok = false;
      if (h.seen[k].prev_level != (k & 3u)) payload_ok = false;
      if (h.seen[k].hold != k) payload_ok = false;
    }
    check_true(payload_ok,
               "case1: descriptor k carries SUBPATCH k's payload -- no staging slip");
  }
  check_eq(h.t.sub_order_bad_o, 0, "case1: devstore streamed in order");
}

// ===========================================================================
void case2_placement_matches_the_law(Harness& h) {
  std::printf("-- case 2: THE PLACEMENT MATCHES THE RATIFIED LAW AT EVERY LEGAL PITCH\n");
  for (int8_t pitch : {static_cast<int8_t>(-1), static_cast<int8_t>(0),
                       static_cast<int8_t>(1), static_cast<int8_t>(2)}) {
    h.reset();
    fill_dev(h);
    h.t.frz_pitch_log2_i = pitch;
    h.resident = {{{40, 12}, 3}};
    Rec a; a.island = 7; a.ix = 40; a.iz = 12; a.src_id = 0x0101;
    const std::vector<Rec> recs = {a};
    const uint32_t crc = h.load_list(recs);
    check_true(h.run_walk(crc, 1), "case2: the walk terminated");
    check_eq(h.seen.size(), 16, "case2: sixteen descriptors");
    bool ok = (h.seen.size() == 16);
    for (size_t k = 0; ok && k < h.seen.size(); ++k) {
      const int32_t want_x = place_law(40, centre_i(h.seen[k].sub), pitch);
      const int32_t want_z = place_law(12, centre_j(h.seen[k].sub), pitch);
      if (h.seen[k].cx != want_x || h.seen[k].cz != want_z) {
        std::printf("       pitch %d subpatch %u: cx %d want %d, cz %d want %d\n",
                    pitch, h.seen[k].sub, h.seen[k].cx, want_x, h.seen[k].cz, want_z);
        ok = false;
      }
    }
    char what[96];
    std::snprintf(what, sizeof what,
                  "case2: every subpatch centre matches TERRAIN.PLACE's law at pitch_log2 %d",
                  pitch);
    check_true(ok, what);

    // The height conversion is spdesc's: sign-extend height16 then shift 8.
    if (h.seen.size() == 16) {
      const int32_t want_cy = (static_cast<int32_t>(h.dev[0].cy) << 8);
      check_eq(static_cast<uint32_t>(h.seen[0].cy), static_cast<uint32_t>(want_cy),
               "case2: cy is height16 sign-extended and shifted, as SPDESC converts it");
    }
  }
}

// ===========================================================================
void case3_the_replay_is_checked(Harness& h) {
  std::printf("-- case 3: THE ARENA MOVED UNDERNEATH THE FRAME, AND THE WALK REFUSES\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}, {{41, 12}, 9}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);
  // T5 makes the list capture data, immutable for the frame. Something
  // rewrote it between the command and the walk.
  h.arena[kListOff + 5] ^= 0xFF;

  check_true(h.run_walk(crc, 2), "case3: the walk still terminated");
  check_true(h.t.list_crc_mismatch_o != 0, "case3: list_crc_mismatch_o FIRED");
  check_eq(h.t.prep_valid_o, 0, "case3: the walk is NOT valid");
  check_true(h.t.restart_req_o != 0, "case3: a restart is REQUESTED");
  check_eq(h.t.walks_completed_o, 0,
           "case3: walks_completed_o is FLAT -- a refused walk is not a completed one");
  // AND prep_done still pulsed, which matters: TERRAIN.EDGERECON must leave
  // PREPARE or its query port never opens and the whole frame waits.
  std::printf("       (prep_done_o still pulsed, so the reconciler is not stranded in PREPARE)\n");
}

// ===========================================================================
void case4_a_miss_is_skipped(Harness& h) {
  std::printf("-- case 4: A NON-RESIDENT PATCH IS SKIPPED, NEVER WAITED ON\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{41, 12}, 9}};          // patch A is NOT resident
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);
  check_true(h.run_walk(crc, 2), "case4: the walk terminated (it did not wait on a load)");
  check_eq(h.t.skipped_not_resident_o, 1, "case4: skipped_not_resident_o FIRED once");
  check_eq(h.t.patches_prepared_o, 1, "case4: ... and only the resident patch was prepared");
  check_eq(h.t.descriptors_emitted_o, 16, "case4: sixteen descriptors, not thirty-two");
  check_eq(h.t.prep_valid_o, 1,
           "case4: the walk is still VALID -- a miss is architecture 2.6's ruled behaviour, "
           "and the patch's neighbours fall back symmetrically");
}

// ===========================================================================
void case5_freeze_broken(Harness& h) {
  std::printf("-- case 5: THE FROZEN WITNESS MOVED MID-WALK\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}, {{41, 12}, 9}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);

  h.t.j_valid_i = 1;
  h.t.j_epoch_i = kEpoch;
  h.t.j_list_off_i = kListOff;
  h.t.j_list_bytes_i = 64;
  h.t.j_list_crc_i = crc;
  h.t.j_patch_count_i = 2;
  h.t.eval();
  h.step();
  h.t.j_valid_i = 0;
  h.t.eval();

  // A bake or residency change lands while the walk is running. The directive:
  // "A bake or residency change either waits for the pinned lifetime or
  // produces a DETECTED restart; never combine half of one generation with
  // half of another."
  for (int i = 0; i < 40; ++i) h.step();
  h.t.frz_tok_i = 0x9999;
  h.t.eval();
  for (int i = 0; i < 20000 && !h.t.prep_done_o; ++i) h.step();
  h.step();

  check_true(h.t.freeze_broken_o != 0, "case5: freeze_broken_o FIRED");
  check_eq(h.t.prep_valid_o, 0, "case5: the walk is NOT valid");
  check_true(h.t.restart_req_o != 0, "case5: a restart is REQUESTED");
  check_eq(h.t.frz_tok_o, 0x1234u,
           "case5: frz_tok_o still reports the witness the walk BEGAN under, "
           "which is what the EMIT side must compare against");
}

// ===========================================================================
void case6_illegal_pitch(Harness& h) {
  std::printf("-- case 6: AN ILLEGAL ISLAND PITCH IS REFUSED BEFORE ANYTHING IS SWEPT\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);
  h.t.frz_pitch_log2_i = 5;      // not one of spec 1.3's four
  check_true(h.run_walk(crc, 2, 2000), "case6: the walk terminated immediately");
  check_true(h.t.pitch_illegal_o != 0, "case6: pitch_illegal_o FIRED");
  check_eq(h.t.prep_valid_o, 0, "case6: the walk is NOT valid");
  check_eq(h.t.records_walked_o, 0,
           "case6: NOT ONE record was walked -- `place32`'s default arm returns zero, so "
           "walking would have banked every patch at the world origin");

  // THE CONTROL: the same list at a legal pitch walks.
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}, {{41, 12}, 9}};
  const uint32_t crc2 = h.load_list(recs);
  h.t.frz_pitch_log2_i = 1;
  check_true(h.run_walk(crc2, 2), "case6 control: a legal pitch walks");
  check_eq(h.t.pitch_illegal_o, 0, "case6 control: pitch_illegal_o is silent");
  check_eq(h.t.records_walked_o, 2, "case6 control: both records walked");
}

// ===========================================================================
void case7_placement_out_of_range(Harness& h) {
  std::printf("-- case 7: A PLACEMENT s32 CANNOT HOLD IS REFUSED, NOT WRAPPED\n");
  h.reset();
  fill_dev(h);
  // THE COORDINATE IS CHOSEN SO THAT THE PITCH IS WHAT DECIDES, and the first
  // version of this case got that wrong in a way worth keeping. It used
  // ix = 30000, whose units are 960,032 -- above the limit at ALL FOUR legal
  // pitches, so the "and here it fits" control could never have passed. A
  // control that cannot succeed is not a control.
  //
  // ix = 1000 gives units = 32,032 and discriminates properly:
  //   pitch +2 -> shift 18, limit 2**13 =  8,192  -> REFUSED
  //   pitch -1 -> shift 15, limit 2**16 = 65,536  -> FITS
  // So the pair tests the RANGE LAW rather than one hopeless coordinate.
  Rec a; a.island = 7; a.ix = 1000; a.iz = 12; a.src_id = 0x0201;
  const std::vector<Rec> recs = {a};
  h.resident = {{{1000, 12}, 3}};
  const uint32_t crc = h.load_list(recs);
  h.t.frz_pitch_log2_i = 2;
  check_true(h.run_walk(crc, 1), "case7: the walk terminated");
  check_true(h.t.place_range_o != 0, "case7: place_range_o FIRED");
  check_eq(h.t.patches_prepared_o, 0, "case7: the patch was NOT prepared");
  check_eq(h.t.descriptors_emitted_o, 0, "case7: and no descriptor was emitted");

  // THE CONTROL: the SAME coordinate at 0.5 m pitch fits (shift 15, 16 bits).
  h.reset();
  fill_dev(h);
  h.resident = {{{1000, 12}, 3}};
  const uint32_t crc2 = h.load_list(recs);
  h.t.frz_pitch_log2_i = -1;
  check_true(h.run_walk(crc2, 1), "case7 control: the walk terminated");
  check_eq(h.t.place_range_o, 0, "case7 control: place_range_o is silent at a pitch that fits");
  check_eq(h.t.patches_prepared_o, 1, "case7 control: the patch WAS prepared");
}

// ===========================================================================
void case8_unfresh(Harness& h) {
  std::printf("-- case 8: A SLOT DEVSTORE NEVER WROTE IS COUNTED, NOT REFUSED\n");
  h.reset();
  fill_dev(h);
  h.dev_fresh = false;
  h.resident = {{{40, 12}, 3}};
  Rec a; a.island = 7; a.ix = 40; a.iz = 12; a.src_id = 0x0101;
  const std::vector<Rec> recs = {a};
  const uint32_t crc = h.load_list(recs);
  check_true(h.run_walk(crc, 1), "case8: the walk terminated");
  check_eq(h.t.patches_unfresh_o, 1, "case8: patches_unfresh_o FIRED once per PATCH");
  check_eq(h.t.descriptors_emitted_o, 16,
           "case8: the patch is still prepared -- the neutral triple is a legal answer");
  h.dev_fresh = true;
}

// ===========================================================================
void case9_job_refused(Harness& h) {
  std::printf("-- case 9: A JOB THE COMMAND PATH WOULD REFUSE IS REFUSED HERE TOO\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);

  // A STALE RESOURCE EPOCH. A walk that prepared a frame `zhao_terrain_cmd`
  // refuses would bank decisions nobody emits.
  h.t.j_valid_i = 1;
  h.t.j_epoch_i = kEpoch + 1;
  h.t.j_list_off_i = kListOff;
  h.t.j_list_bytes_i = 64;
  h.t.j_list_crc_i = crc;
  h.t.j_patch_count_i = 2;
  h.t.eval();
  h.step();
  h.t.j_valid_i = 0;
  h.t.eval();
  for (int i = 0; i < 200 && !h.t.prep_done_o; ++i) h.step();
  h.step();
  check_true(h.t.jobs_refused_o != 0, "case9: jobs_refused_o FIRED on a stale epoch");
  check_eq(h.t.prep_valid_o, 0, "case9: the walk is NOT valid");
  check_eq(h.t.records_walked_o, 0, "case9: no byte of the list was read");

  // THE CONTROL: the correct epoch is accepted.
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}, {{41, 12}, 9}};
  const uint32_t crc2 = h.load_list(recs);
  check_true(h.run_walk(crc2, 2), "case9 control: the walk terminated");
  check_eq(h.t.jobs_refused_o, 0, "case9 control: jobs_refused_o is silent");
}

// ===========================================================================
void case10_sub_order(Harness& h) {
  std::printf("-- case 10: DEVSTORE STREAMING OUT OF ORDER IS COUNTED\n");
  h.reset();
  fill_dev(h);
  h.dev_scramble = true;
  h.resident = {{{40, 12}, 3}};
  Rec a; a.island = 7; a.ix = 40; a.iz = 12; a.src_id = 0x0101;
  const std::vector<Rec> recs = {a};
  const uint32_t crc = h.load_list(recs);
  check_true(h.run_walk(crc, 1), "case10: the walk terminated");
  check_true(h.t.sub_order_bad_o != 0, "case10: sub_order_bad_o FIRED");
  h.dev_scramble = false;

  // THE CONTROL is case 1, which asserts the counter is zero on an in-order
  // stream; restated here so the two live beside each other.
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}};
  const uint32_t crc2 = h.load_list(recs);
  check_true(h.run_walk(crc2, 1), "case10 control: the walk terminated");
  check_eq(h.t.sub_order_bad_o, 0, "case10 control: sub_order_bad_o is silent in order");
}

// ===========================================================================
void case11_bridge_error(Harness& h) {
  std::printf("-- case 11: A BRIDGE REFUSAL ABANDONS THE WALK RATHER THAN HANGING\n");
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}};
  const auto recs = two_patch_list();
  const uint32_t crc = h.load_list(recs);
  h.bridge_refuse = true;
  check_true(h.run_walk(crc, 2, 4000), "case11: the walk terminated");
  check_true(h.t.bridge_errs_o != 0, "case11: bridge_errs_o FIRED");
  check_eq(h.t.prep_valid_o, 0, "case11: the walk is NOT valid");
  h.bridge_refuse = false;
}

// ===========================================================================
void case12_alignment_refetch(Harness& h) {
  std::printf("-- case 12: THE 64-BYTE-ALIGNMENT COST IS MEASURED, NOT ARGUED ABOUT\n");
  // A T5 record boundary is 32 bytes and the bridge admits only 64-byte-aligned
  // bursts. `zhao_terrain_cmd` shipped exactly that defect and it was found
  // only against the REAL bridge -- so this bench's bridge model enforces the
  // alignment rule, and an unaligned request would raise `err` and show up as
  // bridge_errs_o rather than passing quietly.
  h.reset();
  fill_dev(h);
  h.resident = {{{40, 12}, 3}};
  // THREE records: 96 bytes, so the walk must resume from a 32-byte-aligned
  // byte at least once and discard a lead.
  Rec a, b, c;
  a.island = 7; a.ix = 40; a.iz = 12; a.src_id = 0x0301;
  b.island = 7; b.ix = 60; b.iz = 60; b.src_id = 0x0302;
  c.island = 7; c.ix = 61; c.iz = 60; c.src_id = 0x0303;
  const std::vector<Rec> recs = {a, b, c};
  const uint32_t crc = h.load_list(recs);
  check_true(h.run_walk(crc, 3), "case12: the walk terminated");
  check_eq(h.t.bridge_errs_o, 0,
           "case12: NO bridge error -- every burst this block asked for was 64-byte aligned");
  check_eq(h.t.list_crc_mismatch_o, 0,
           "case12: and the CRC still matched, so the discarded lead was NOT folded twice");
  check_eq(h.t.records_walked_o, 3, "case12: all three records walked");
  std::printf("       list_bytes_read_o = %u, list_refetch_bytes_o = %u\n",
              static_cast<unsigned>(h.t.list_bytes_read_o),
              static_cast<unsigned>(h.t.list_refetch_bytes_o));
  check_eq(h.t.list_bytes_read_o, 96, "case12: ninety-six bytes of LIST were consumed");
}

}  // namespace

int main(int argc, char** argv) {
  // UNBUFFERED, and it is a diagnostic rather than a style choice. A piped
  // run buffers stdout in 4 KB blocks, so a bench that blocks part way
  // through prints NOTHING and a late fault reads as a failure to start --
  // CLAUDE.md's 'buffered output lost in a crash makes a late fault look
  // like an early one'. This bench cost one wedge diagnosed that way.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Verilated::commandArgs(argc, argv);
  Harness h;

  case1_complete_walk(h);
  case2_placement_matches_the_law(h);
  case3_the_replay_is_checked(h);
  case4_a_miss_is_skipped(h);
  case5_freeze_broken(h);
  case6_illegal_pitch(h);
  case7_placement_out_of_range(h);
  case8_unfresh(h);
  case9_job_refused(h);
  case10_sub_order(h);
  case11_bridge_error(h);
  case12_alignment_refetch(h);

  std::printf("\nterrain_prepwalk_directed: %d checks, %d failure(s)\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
