// field_stamp_adapter_directed.cpp -- the S profile's stream adapter.
//
// TWO QUESTIONS, AND THEY ARE NOT THE SAME QUESTION.
//
//   1. ALIGNMENT. Does the walk produce one record per visited texel in
//      `zhao_surface_stamp`'s own j-outer/i-inner order, and stay in step with
//      the consumer across a restart and a fault? This is all the LEGACY
//      binding can get wrong, because it computes nothing.
//   2. CONVERSION. Does `CANONICAL_STAMP` implement owner directive 15.2's two
//      written conversions exactly -- the texel centre on the way in and the
//      unit-to-u16 mapping on the way out?
//
// **AND THE THIRD THING, WHICH IS THE ONE THAT MATTERS MOST.** 15.2 says: "The
// legacy bridge is not evidence that the full canonical Stamp binding works."
// So this file drives TWO SEPARATELY ELABORATED INSTANCES
// (tests/field/tb_field_stamp_bindings.sv) with independent stimulus, and
// every assertion names which binding it is about. A pass on `u_legacy` leaves
// every `u_canonical` counter at rest, and case D asserts that it does.
//
// ===========================================================================
// THE CASE THAT DISCRIMINATES THE TWO BINDINGS
// ===========================================================================
// CASE C1. Both instances are handed the SAME engine response, carrying unit
// strength 1.0 on canonical ordinal 1 -- fx16 `0x0001_0000`.
//
//     CANONICAL_STAMP delivers  fld_strength_o = 65535   (full brush)
//     LEGACY_STAMP_BRUSH delivers fld_strength_o = 0     (no brush at all)
//
// because the legacy bridge takes the LOW 16 BITS, and the low half of fx16
// 1.0 is zero. This is not a subtlety I chose; it is the failure owner
// directive 15.2 names in so many words -- "do not break the existing stamp
// pictures ... by taking the low 16 bits of Q16.16 1.0 (which is zero)" -- and
// it is the maximum possible disagreement between the two contracts, from one
// identical response.
//
// CASE C2 is the same discrimination on the INPUT side: for texel 0 the
// canonical binding offers R0 = 512 (the texel CENTRE, (2*0+1)*512) while the
// legacy binding offers R0 = 0 (the raw index).
//
// Either case alone proves the bindings are two contracts rather than one
// contract with two names. Both are asserted.
//
// ===========================================================================
// WHY THE ENGINE IS A MOCK
// ===========================================================================
// A real `zhao_field_host` would make each case a hundred times slower and
// would re-answer a question `field_host_directed` already answers. What the
// mock adds is the ability to return a CHOSEN unit value and a FAULT on cue --
// and a conversion test needs an exact input value, which a loaded program
// cannot be made to produce without choosing an ABI inside a test.
//
// The mock is written to the shared engine's real protocol -- one request in
// flight, the answer HELD until taken -- because a mock that dropped an untaken
// answer would have hidden the restart defect this driver originally found.
//
// The mock also RECORDS the lanes it was asked for, so the order and the texel
// centres are checked against the consumer's law rather than against this
// file's opinion of it. A test that regenerated the expected order with the
// same expression the RTL uses would agree with itself.
//
//   A  LEGACY  alignment: 4,096 records, order, restart, fault, arming
//   B  CANONICAL conversions: both endpoints, the half case, out of range
//   C  THE DISCRIMINATOR: one response, two different records
//   D  NEGATIVE CONTROL: exercising legacy marks nothing about canonical

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_field_stamp_bindings.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kSheetW = 64;
constexpr int kSheetH = 64;
constexpr int kTexels = kSheetW * kSheetH;

// 15.2's worked example, for a 64-wide sheet: the unit centre of column i is
// (2i+1)/128, which in fx16 is exactly (2i+1)*512.
constexpr uint32_t kCentreStep = 512;

constexpr uint32_t kFx16One = 0x00010000u;
constexpr uint32_t kFx16Half = 0x00008000u;

using Dut = Vtb_field_stamp_bindings;

void step(Dut& d) { zhao::tick(d); }

// Which instance a helper is talking to. The two are never driven by the same
// call, which is what keeps case D honest.
enum class Bind { Legacy, Canonical };

void reset(Dut& d) {
  d.rst_n = 0;
  d.l_cmd_fire_i = 0;  d.c_cmd_fire_i = 0;
  d.l_cmd_field_en_i = 0;  d.c_cmd_field_en_i = 0;
  d.l_slot_i = 2;  d.c_slot_i = 2;
  d.l_slot_valid_i = 1;  d.c_slot_valid_i = 1;
  d.l_fld_ready_i = 0;  d.c_fld_ready_i = 0;
  d.l_req_ready_i = 0;  d.c_req_ready_i = 0;
  d.l_resp_valid_i = 0;  d.c_resp_valid_i = 0;
  d.l_resp_status_i = 0;  d.c_resp_status_i = 0;
  for (int i = 0; i < 7; ++i) {
    d.l_resp_out_i[i] = 0;
    d.c_resp_out_i[i] = 0;
  }
  d.eval();
  for (int i = 0; i < 3; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

struct Log {
  std::vector<uint32_t> u;          // canonical ordinal 0 of the REQUEST (R0)
  std::vector<uint32_t> v;          // R1
  std::vector<uint32_t> tag_op;
  std::vector<uint32_t> strength;
  std::vector<uint32_t> emissive;
  int records = 0;
};

// The engine response this pump will hand back. `tag_op_echo` makes the answer
// encode the point it was asked about, so a record that reaches the consumer
// can be traced to the request that produced it -- a constant would make every
// record indistinguishable, which is the shape of the cursor bug being hunted.
struct Resp {
  bool tag_op_echo = true;
  uint32_t tag_op = 0;
  uint32_t strength = 0;
  uint32_t emissive = 0;
  int fault_at = -1;
};

void pump(Dut& d, Bind b, Log& log, int budget, const Resp& r) {
  for (int cycle = 0; cycle < budget; ++cycle) {
    d.eval();
    const bool req_v = (b == Bind::Legacy) ? d.l_req_valid_o : d.c_req_valid_o;
    const bool resp_v = (b == Bind::Legacy) ? d.l_resp_valid_i : d.c_resp_valid_i;
    const bool can_accept = req_v && !resp_v;
    if (b == Bind::Legacy) { d.l_req_ready_i = can_accept ? 1 : 0; d.l_fld_ready_i = 1; }
    else                   { d.c_req_ready_i = can_accept ? 1 : 0; d.c_fld_ready_i = 1; }
    d.eval();

    uint32_t u = 0, v = 0;
    int index = -1;
    if (can_accept) {
      u = (b == Bind::Legacy) ? d.l_req_in_o[0] : d.c_req_in_o[0];
      v = (b == Bind::Legacy) ? d.l_req_in_o[1] : d.c_req_in_o[1];
      index = static_cast<int>(log.u.size());
      log.u.push_back(u);
      log.v.push_back(v);
    }
    const bool resp_taken = (b == Bind::Legacy) ? (d.l_resp_valid_i && d.l_resp_ready_o)
                                                : (d.c_resp_valid_i && d.c_resp_ready_o);
    const bool fld_v = (b == Bind::Legacy) ? d.l_fld_valid_o : d.c_fld_valid_o;
    const bool fld_r = (b == Bind::Legacy) ? d.l_fld_ready_i : d.c_fld_ready_i;
    if (fld_v && fld_r) {
      log.tag_op.push_back((b == Bind::Legacy) ? d.l_fld_tag_op_o : d.c_fld_tag_op_o);
      log.strength.push_back((b == Bind::Legacy) ? d.l_fld_strength_o : d.c_fld_strength_o);
      log.emissive.push_back((b == Bind::Legacy) ? d.l_fld_emissive_o : d.c_fld_emissive_o);
      log.records++;
    }

    step(d);

    if (resp_taken) {
      if (b == Bind::Legacy) d.l_resp_valid_i = 0; else d.c_resp_valid_i = 0;
    }
    if (can_accept) {
      // The RESPONSE is written in CANONICAL OUTPUT ORDINALS -- ordinal 0
      // tag_op, 1 strength, 2 emissive -- because that is the one result
      // contract FH17 requires every consumer to speak. The mock does not
      // model a physical capture window; that translation is the host's.
      const uint32_t tag = r.tag_op_echo ? (((v & 0xFFFFu) << 16) | (u & 0xFFFFu)) : r.tag_op;
      const uint8_t st = (index == r.fault_at) ? 0xF0 : 0x00;
      if (b == Bind::Legacy) {
        d.l_resp_out_i[0] = tag;
        d.l_resp_out_i[1] = r.strength;
        d.l_resp_out_i[2] = r.emissive;
        d.l_resp_status_i = st;
        d.l_resp_valid_i = 1;
      } else {
        d.c_resp_out_i[0] = tag;
        d.c_resp_out_i[1] = r.strength;
        d.c_resp_out_i[2] = r.emissive;
        d.c_resp_status_i = st;
        d.c_resp_valid_i = 1;
      }
    }
  }
  if (b == Bind::Legacy) { d.l_req_ready_i = 0; d.l_fld_ready_i = 0; }
  else                   { d.c_req_ready_i = 0; d.c_fld_ready_i = 0; }
}

void fire_command(Dut& d, Bind b, bool field_en) {
  if (b == Bind::Legacy) { d.l_cmd_fire_i = 1; d.l_cmd_field_en_i = field_en ? 1 : 0; }
  else                   { d.c_cmd_fire_i = 1; d.c_cmd_field_en_i = field_en ? 1 : 0; }
  step(d);
  if (b == Bind::Legacy) { d.l_cmd_fire_i = 0; d.l_cmd_field_en_i = 0; }
  else                   { d.c_cmd_fire_i = 0; d.c_cmd_field_en_i = 0; }
}

// One texel's worth of traffic: start a walk and take the FIRST record.
// Returns that record's delivered fields plus the lanes it was asked on.
struct First {
  uint32_t r0 = 0xFFFFFFFFu;
  uint32_t r1 = 0xFFFFFFFFu;
  uint32_t strength = 0xFFFFFFFFu;
  uint32_t emissive = 0xFFFFFFFFu;
  bool got = false;
};

First first_record(Dut& d, Bind b, uint32_t unit_strength, uint32_t unit_emissive) {
  Resp r;
  r.strength = unit_strength;
  r.emissive = unit_emissive;
  fire_command(d, b, true);
  Log log;
  pump(d, b, log, 60, r);
  First f;
  if (log.records > 0 && !log.u.empty()) {
    f.r0 = log.u[0];
    f.r1 = log.v[0];
    f.strength = log.strength[0];
    f.emissive = log.emissive[0];
    f.got = true;
  }
  return f;
}

// Abandon a part-finished walk so the next case starts from an idle block.
void quiesce(Dut& d, Bind b) {
  Resp r;
  fire_command(d, b, false);
  Log drain;
  pump(d, b, drain, kTexels * 24, r);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut);

  // =========================================================================
  // A. LEGACY_STAMP_BRUSH -- alignment. Every pre-2026-09-20 case, unchanged.
  // =========================================================================

  // ---- A6. residency gates the arm ---------------------------------------
  dut.l_slot_valid_i = 0;
  dut.eval();
  check(dut.l_arm_ready_o == 0, "LEGACY: arm_ready_o low with no resident program", 0,
        dut.l_arm_ready_o);
  dut.l_slot_valid_i = 1;
  dut.eval();
  check(dut.l_arm_ready_o == 1, "LEGACY: arm_ready_o high once a program is resident", 1,
        dut.l_arm_ready_o);

  // ---- A3. a stamp that did not ask for the brush produces nothing --------
  fire_command(dut, Bind::Legacy, /*field_en=*/false);
  {
    Log quiet;
    Resp r;
    pump(dut, Bind::Legacy, quiet, 200, r);
    check(quiet.records == 0, "LEGACY: no records for a stamp that did not ask", 0,
          static_cast<uint64_t>(quiet.records));
    check(dut.l_stamps_o == 0, "LEGACY: stamps_o did not move", 0, dut.l_stamps_o);
    check(dut.l_busy_o == 0, "LEGACY: the adapter stayed idle", 0, dut.l_busy_o);
  }

  // ---- A1 and A2. a whole walk, in the consumer's order -------------------
  fire_command(dut, Bind::Legacy, /*field_en=*/true);
  check(dut.l_stamps_o == 1, "LEGACY: stamps_o fired", 1, dut.l_stamps_o);
  Log walk;
  {
    Resp r;
    pump(dut, Bind::Legacy, walk, kTexels * 24, r);
  }
  check(walk.records == kTexels, "LEGACY: exactly 4,096 records", kTexels,
        static_cast<uint64_t>(walk.records));
  check(dut.l_texels_o == static_cast<uint32_t>(kTexels), "LEGACY: texels_o counted them",
        kTexels, dut.l_texels_o);
  check(dut.l_busy_o == 0, "LEGACY: the walk ended", 0, dut.l_busy_o);

  // j-outer, i-inner: u runs 0..63 for each v, and v advances once per row.
  // Checked as a sequence rather than recomputed from the same expression the
  // RTL uses, because a test that re-derives the order agrees with itself.
  // AND THE VALUES ARE RAW INDICES, which is the legacy contract.
  bool order_ok = (static_cast<int>(walk.u.size()) == kTexels);
  if (order_ok) {
    for (int t = 0; t < kTexels; ++t) {
      if (walk.u[t] != static_cast<uint32_t>(t % kSheetW) ||
          walk.v[t] != static_cast<uint32_t>(t / kSheetW)) {
        order_ok = false;
        std::printf("  first order break at t=%d: u=%u v=%u\n", t, walk.u[t], walk.v[t]);
        break;
      }
    }
  }
  check(order_ok, "LEGACY: the walk is j-outer / i-inner, u fast, RAW INDICES", 1,
        order_ok ? 1 : 0);

  bool payload_ok = (static_cast<int>(walk.tag_op.size()) == kTexels);
  if (payload_ok) {
    for (int t = 0; t < kTexels; ++t) {
      const uint32_t want = (static_cast<uint32_t>(t / kSheetW) << 16) |
                            static_cast<uint32_t>(t % kSheetW);
      if (walk.tag_op[t] != want) {
        payload_ok = false;
        std::printf("  first payload break at t=%d: got 0x%08X want 0x%08X\n", t,
                    walk.tag_op[t], want);
        break;
      }
    }
  }
  check(payload_ok, "LEGACY: record t carries the answer to request t", 1,
        payload_ok ? 1 : 0);

  // ---- A4. a command mid-walk restarts the cursor -------------------------
  fire_command(dut, Bind::Legacy, true);
  Log partial;
  {
    Resp r;
    pump(dut, Bind::Legacy, partial, 400, r);
  }
  check(partial.records > 0 && partial.records < kTexels, "LEGACY: a walk is in progress", 1,
        static_cast<uint64_t>(partial.records));
  const uint32_t restarts_before = dut.l_restarts_o;
  fire_command(dut, Bind::Legacy, true);
  check(dut.l_restarts_o == restarts_before + 1, "LEGACY: restarts_o fired",
        restarts_before + 1, dut.l_restarts_o);
  Log after;
  {
    Resp r;
    pump(dut, Bind::Legacy, after, 400, r);
  }
  check(!after.u.empty() && after.u[0] == 0 && after.v[0] == 0,
        "LEGACY: the cursor restarted at (0,0)", 0,
        after.u.empty() ? 0xFFFFFFFFu : ((after.v[0] << 16) | after.u[0]));

  {
    Resp r;
    Log drain;
    pump(dut, Bind::Legacy, drain, kTexels * 24, r);
  }
  check(dut.l_busy_o == 0, "LEGACY: the restarted walk completed", 0, dut.l_busy_o);

  // ---- A5. a faulting run still delivers its record -----------------------
  check(dut.l_faults_o == 0, "LEGACY: faults_o starts at zero", 0, dut.l_faults_o);
  fire_command(dut, Bind::Legacy, true);
  Log faulty;
  {
    Resp r;
    r.fault_at = 7;
    pump(dut, Bind::Legacy, faulty, kTexels * 24, r);
  }
  check(dut.l_faults_o == 1, "LEGACY: faults_o fired exactly once", 1, dut.l_faults_o);
  check(faulty.records == kTexels, "LEGACY: the faulting walk still delivered every record",
        kTexels, static_cast<uint64_t>(faulty.records));
  check(faulty.tag_op.size() > 7 && faulty.tag_op[7] == 0,
        "LEGACY: the faulted record is ZEROED, and the counter is what says so", 0,
        faulty.tag_op.size() > 7 ? faulty.tag_op[7] : 0xFFFFFFFFu);

  // The legacy binding performs NO unit conversion, so its clamp counter is
  // structurally unreachable. Asserted here so that the zero below in case D
  // is known to be a property of the binding and not of the stimulus.
  check(dut.l_canon_clamps_o == 0,
        "LEGACY: canon_clamps_o is structurally zero -- this binding converts nothing", 0,
        dut.l_canon_clamps_o);

  // =========================================================================
  // B. CANONICAL_STAMP -- owner directive 15.2's two written conversions.
  // =========================================================================

  // ---- B1. the texel centre, on the way in -------------------------------
  // 15.2: "a 64-wide unit center (2*i+1)/128 maps exactly to fx16 (2*i+1)*512".
  // Texel 0 is therefore 512 and NOT 0: a centre is never on a sheet edge,
  // which is the entire point of the convention.
  {
    First f = first_record(dut, Bind::Canonical, kFx16One, 0);
    check(f.got, "CANONICAL: a record was delivered", 1, f.got ? 1 : 0);
    check(f.r0 == kCentreStep, "CANONICAL: texel 0's u is the CENTRE (2*0+1)*512 = 512",
          kCentreStep, f.r0);
    check(f.r1 == kCentreStep, "CANONICAL: texel 0's v is the CENTRE 512", kCentreStep, f.r1);
    quiesce(dut, Bind::Canonical);
  }

  // The far end of the sheet, taken from a full row of the walk: column 63's
  // centre is (2*63+1)*512 = 65024, which is just below fx16 1.0 and not equal
  // to it. Both endpoints of the conversion, as 15.2 asks.
  {
    fire_command(dut, Bind::Canonical, true);
    Log row;
    Resp r;
    r.strength = kFx16Half;
    pump(dut, Bind::Canonical, row, 64 * 24, r);
    bool centres_ok = (row.u.size() >= static_cast<size_t>(kSheetW));
    if (centres_ok) {
      for (int i = 0; i < kSheetW; ++i) {
        const uint32_t want = static_cast<uint32_t>(2 * i + 1) * kCentreStep;
        if (row.u[i] != want) {
          centres_ok = false;
          std::printf("  centre break at i=%d: got %u want %u\n", i, row.u[i], want);
          break;
        }
      }
    }
    check(centres_ok, "CANONICAL: every column's u is its texel centre, i=0..63", 1,
          centres_ok ? 1 : 0);
    check(row.u.size() >= static_cast<size_t>(kSheetW) && row.u[63] == 65024u,
          "CANONICAL: column 63's centre is 65024, just under fx16 1.0", 65024,
          row.u.size() > 63 ? row.u[63] : 0xFFFFFFFFu);
    quiesce(dut, Bind::Canonical);
  }

  // ---- B2. unit -> u16, on the way out ------------------------------------
  // 15.2: strength16 = floor((clamp(s,0,65536) * 65535 + 32768) / 65536),
  // "test both endpoints, half cases and out-of-range values".
  struct ConvCase {
    uint32_t in;
    uint32_t want;
    bool clamps;
    const char* name;
  };
  const ConvCase conv[] = {
    {0x00000000u,     0u,     false, "CANONICAL: fx16 0.0 -> 0 (the low endpoint)"},
    {kFx16One,    65535u,     false, "CANONICAL: fx16 1.0 -> 65535 (the high endpoint)"},
    {kFx16Half,   32768u,     false, "CANONICAL: fx16 0.5 -> 32768 exactly (the half case)"},
    {0x00000001u,     1u,     false, "CANONICAL: one ulp above zero rounds to 1, not 0"},
    {0x00020000u, 65535u,     true,  "CANONICAL: fx16 2.0 is CLAMPED to 65535"},
    {0xFFFFFFFFu,     0u,     true,  "CANONICAL: a NEGATIVE unit is CLAMPED to 0"},
  };
  for (const ConvCase& c : conv) {
    const uint32_t clamps_before = dut.c_canon_clamps_o;
    First f = first_record(dut, Bind::Canonical, c.in, 0);
    check(f.got && f.strength == c.want, c.name, c.want, f.got ? f.strength : 0xFFFFFFFFu);
    if (c.clamps) {
      // MOVED, not "moved by exactly one". `first_record` reads the FIRST
      // record but the walk keeps going for the rest of the pump window, and
      // every response in it carries the same out-of-range value, so the
      // counter advances once per response. Asserting an exact delta here
      // would be asserting the pump's cycle budget, not the RTL. The
      // discriminating half is the ELSE branch below, which requires the
      // counter not to move at ALL on an in-range value -- and that is the
      // polarity a broken clamp detector would fail.
      check(dut.c_canon_clamps_o > clamps_before,
            "CANONICAL: canon_clamps_o FIRED on the out-of-range value",
            clamps_before + 1, dut.c_canon_clamps_o);
    } else {
      check(dut.c_canon_clamps_o == clamps_before,
            "CANONICAL: canon_clamps_o stayed put on an in-range value", clamps_before,
            dut.c_canon_clamps_o);
    }
    quiesce(dut, Bind::Canonical);
  }

  // ---- B3. emissive is canonical ordinal 2, returned and converted --------
  // 15.2: "the host returns emissive as the third declared output, even where
  // the present surface consumer has no emissive input."
  {
    First f = first_record(dut, Bind::Canonical, kFx16Half, kFx16One);
    check(f.got && f.emissive == 65535u,
          "CANONICAL: emissive (ordinal 2) is returned and converted, fx16 1.0 -> 65535",
          65535, f.got ? f.emissive : 0xFFFFFFFFu);
    check(f.strength == 32768u,
          "CANONICAL: ... and it did not disturb ordinal 1", 32768, f.strength);
    quiesce(dut, Bind::Canonical);
  }
  {
    First f = first_record(dut, Bind::Legacy, kFx16Half, kFx16One);
    check(f.got && f.emissive == 0u,
          "LEGACY: declares TWO outputs, so emissive is held at zero", 0,
          f.got ? f.emissive : 0xFFFFFFFFu);
    quiesce(dut, Bind::Legacy);
  }

  // =========================================================================
  // C. THE DISCRIMINATOR. One identical response; two different records.
  // =========================================================================
  // This is the case the receipt names. If these two ever agree, the bindings
  // have collapsed into one and every assertion above is measuring one machine
  // twice.
  {
    const uint32_t clamps_before = dut.c_canon_clamps_o;

    First lf = first_record(dut, Bind::Legacy, kFx16One, 0);
    quiesce(dut, Bind::Legacy);
    First cf = first_record(dut, Bind::Canonical, kFx16One, 0);
    quiesce(dut, Bind::Canonical);

    check(lf.got && cf.got, "C: both bindings answered the same response", 1,
          (lf.got && cf.got) ? 1 : 0);

    // C1 -- the OUTPUT side. 15.2's named failure, executed.
    check(lf.strength == 0u,
          "C1 LEGACY: fx16 1.0's low 16 bits are ZERO -- no brush at all", 0,
          lf.strength);
    check(cf.strength == 65535u,
          "C1 CANONICAL: the same response is a FULL brush, 65535", 65535, cf.strength);
    check(lf.strength != cf.strength,
          "C1 THE DISCRIMINATOR: one response, two contracts, opposite ends of the range",
          1, (lf.strength != cf.strength) ? 1 : 0);

    // C2 -- the INPUT side.
    check(lf.r0 == 0u, "C2 LEGACY: texel 0 offers R0 = 0, the raw index", 0, lf.r0);
    check(cf.r0 == kCentreStep, "C2 CANONICAL: texel 0 offers R0 = 512, the centre",
          kCentreStep, cf.r0);
    check(lf.r0 != cf.r0, "C2 THE DISCRIMINATOR, on the input side too", 1,
          (lf.r0 != cf.r0) ? 1 : 0);

    // Neither of these is a clamp: fx16 1.0 is the endpoint and IS in range.
    check(dut.c_canon_clamps_o == clamps_before,
          "C: the endpoint 1.0 is IN range and does not count as a clamp", clamps_before,
          dut.c_canon_clamps_o);
  }

  // =========================================================================
  // D. THE NEGATIVE CONTROL. Legacy passing marks NOTHING about canonical.
  // =========================================================================
  // 15.2: "The legacy bridge is not evidence that the full canonical Stamp
  // binding works." Section A above ran 4,096-record walks, a restart and a
  // fault entirely on `u_legacy`. If those had been able to move a canonical
  // counter, the two instances would be sharing state and every "CANONICAL:"
  // assertion above would be suspect.
  {
    const uint32_t c_texels = dut.c_texels_o;
    const uint32_t c_stamps = dut.c_stamps_o;
    const uint32_t c_faults = dut.c_faults_o;
    const uint32_t c_restarts = dut.c_restarts_o;

    fire_command(dut, Bind::Legacy, true);
    Log big;
    Resp r;
    r.fault_at = 3;
    pump(dut, Bind::Legacy, big, kTexels * 24, r);

    check(big.records == kTexels, "D: a further full LEGACY walk ran", kTexels,
          static_cast<uint64_t>(big.records));
    check(dut.c_texels_o == c_texels, "D: ... and CANONICAL texels_o did not move",
          c_texels, dut.c_texels_o);
    check(dut.c_stamps_o == c_stamps, "D: ... nor stamps_o", c_stamps, dut.c_stamps_o);
    check(dut.c_faults_o == c_faults, "D: ... nor faults_o, though LEGACY faulted",
          c_faults, dut.c_faults_o);
    check(dut.c_restarts_o == c_restarts, "D: ... nor restarts_o", c_restarts,
          dut.c_restarts_o);
    check(dut.c_busy_o == 0, "D: the canonical instance was idle throughout", 0,
          dut.c_busy_o);
  }

  // And the converse, so the control is not one-directional -- the defect
  // CLAUDE.md records as "a one-directional ledger check".
  {
    const uint32_t l_texels = dut.l_texels_o;
    const uint32_t l_stamps = dut.l_stamps_o;

    First f = first_record(dut, Bind::Canonical, kFx16Half, 0);
    check(f.got, "D: a CANONICAL record was delivered", 1, f.got ? 1 : 0);
    check(dut.l_texels_o == l_texels, "D: ... and LEGACY texels_o did not move", l_texels,
          dut.l_texels_o);
    check(dut.l_stamps_o == l_stamps, "D: ... nor stamps_o", l_stamps, dut.l_stamps_o);
    quiesce(dut, Bind::Canonical);
  }

  return zhao::report_and_exit("field_stamp_adapter_directed");
}
