// field_v2_lanemux_directed.cpp — v2's tagged lane serialiser.
//
// FIELD v2 issues VECTOR instructions; every long-operation unit in the engine
// is SCALAR. This block turns one into the other, and the properties worth
// pinning are the ones v1 never had to have:
//
//   1. TAG CONSERVATION. A reply must carry back the wavefront and destination
//      that ASKED. v1 needed no tags -- one instruction in flight meant a reply
//      could only belong to the one thing waiting. With several wavefronts
//      sharing a unit, an untagged reply is a reply to whoever happens to be
//      waiting, and that is the defect class this redesign exists to abolish.
//
//   2. LANE ORDER. Lane k's answer must land in lane k's slot. Off-by-one in
//      the collect index is invisible when every lane carries the same value,
//      so every case here gives the lanes DISTINCT values.
//
//   3. NO REPLY IS INVENTED OR LOST. Requests in, replies out, one for one,
//      with the counts checked rather than assumed.
//
//   4. THE SERIALISATION COST IS MEASURED, not asserted. A shared scalar unit
//      serving LANES lanes costs LANES x II per vector instruction. That number
//      is the reason reports/FIELD_V2_MODEL.md puts CURVE first in the work
//      order, so the test reports it rather than leaving it to argument.
//
// The scalar unit here is a MODEL with a settable latency, not a real Field
// unit: this block's job is routing and tagging, and a real unit would test the
// unit's arithmetic instead of this block's bookkeeping.

#include "Vzhao_field_v2_lanemux.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

namespace {

using zhao::check;

constexpr int kLanes = 4;

// Every wavefront index 0..7 and destination 0..63 is a LEGAL tag, so there is
// no impossible value to park on the request lines. The COMPLEMENT is used
// instead: 7-wf can never equal wf (that needs wf = 3.5) and 63-dst can never
// equal dst. So the poison is guaranteed to differ from the tag it replaces,
// for every transaction, without relying on a value the test happens not to use.
constexpr int poison_wf(int wf) { return 7 - wf; }
constexpr int poison_dst(int dst) { return 63 - dst; }

// A scalar unit with a fixed latency and a ready-when-idle handshake -- the
// same shape every long unit in the engine actually has.
struct ScalarUnit {
  int latency;
  int busy = 0;
  int32_t held = 0;
  bool has_result = false;
  int32_t result = 0;
  uint64_t accepted = 0;
  uint64_t replied = 0;

  explicit ScalarUnit(int lat) : latency(lat) {}

  bool ready() const { return busy == 0 && !has_result; }

  void accept(int32_t a) {
    held = a;
    busy = latency;
    ++accepted;
  }

  // The model's "arithmetic": a value the test can predict exactly, chosen so a
  // lane swap or a dropped tag cannot coincidentally produce the right answer.
  static int32_t f(int32_t a) { return a * 3 + 7; }

  void tick(bool taking) {
    if (has_result && taking) {
      has_result = false;
      ++replied;
    }
    if (busy > 0) {
      if (--busy == 0) {
        result = f(held);
        has_result = true;
      }
    }
  }
};

struct Bench {
  Vzhao_field_v2_lanemux& d;
  ScalarUnit unit;

  Bench(Vzhao_field_v2_lanemux& dut, int latency) : d(dut), unit(latency) {
    d.rst_n = 0;
    d.req_valid_i = 0;
    d.rsp_ready_i = 1;
    d.u_ready_i = 0;
    d.u_rvalid_i = 0;
    d.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
  }

  void drive_unit() {
    d.u_ready_i = unit.ready() ? 1 : 0;
    d.u_rvalid_i = unit.has_result ? 1 : 0;
    d.u_result_i = static_cast<uint32_t>(unit.result);
    d.eval();
  }

  // What the unit was actually shown, in acceptance order. The serialiser
  // issues lane 0 first, so obs[k] is lane k's bundle.
  struct Obs {
    int32_t a, a1, a2, b0, b1;
    int mode, unit;
    uint32_t imm;
  };
  std::vector<Obs> obs;

  void step() {
    drive_unit();
    const bool accepting = d.u_valid_o && unit.ready();
    const int32_t a = static_cast<int32_t>(d.u_a_o);
    const Obs seen{static_cast<int32_t>(d.u_a_o),  static_cast<int32_t>(d.u_a1_o),
                   static_cast<int32_t>(d.u_a2_o), static_cast<int32_t>(d.u_b0_o),
                   static_cast<int32_t>(d.u_b1_o), static_cast<int>(d.u_mode_o),
                   static_cast<int>(d.u_unit_o),   static_cast<uint32_t>(d.u_imm_o)};
    const bool taking = d.u_rready_o && unit.has_result;
    zhao::tick(d);
    if (accepting) {
      unit.accept(a);
      obs.push_back(seen);
    }
    unit.tick(taking);
    drive_unit();
  }

  /** Push one vector request and run until its reply is taken. Returns clocks. */
  int transact(int wf, int dst, int mode, const int32_t* a, int32_t* y, int guard = 4096,
               int unit_sel = 0, uint32_t imm = 0) {
    d.req_wf_i = wf;
    d.req_dst_i = dst;
    d.req_mode_i = mode;
    d.req_unit_i = unit_sel;
    d.req_imm_i = imm;
    for (int l = 0; l < kLanes; ++l) {
      d.req_a_i[l] = static_cast<uint32_t>(a[l]);
      // The rest of the bundle is derived from a0 so every component of every
      // lane is a different number. A component crossed with another -- a1 read
      // where a2 was meant, or lane 2's b0 handed to lane 3 -- then cannot land
      // on a value that happens to be right anyway.
      d.req_a1_i[l] = static_cast<uint32_t>(a[l] + 1000);
      d.req_a2_i[l] = static_cast<uint32_t>(a[l] + 2000);
      d.req_b0_i[l] = static_cast<uint32_t>(a[l] + 3000);
      d.req_b1_i[l] = static_cast<uint32_t>(a[l] + 4000);
    }
    d.req_valid_i = 1;
    drive_unit();
    int clocks = 0;
    while (!d.req_ready_o && clocks < guard) {
      step();
      ++clocks;
    }
    step();
    ++clocks;

    // POISON THE REQUEST LINES AFTER THE ACCEPT. Dropping req_valid_i is not
    // enough: the tag inputs keep their old values, so a reply tagged from the
    // LIVE input reads the same thing as one tagged from the CAPTURED copy and
    // the difference is invisible. Mutants M85/M86 -- reply tagged live rather
    // than carried -- survived the whole suite for exactly that reason, and the
    // test claimed to prove tag conservation while proving nothing of the sort.
    //
    // Poisoning with values that cannot be any legal in-flight tag means a
    // live-sourced tag now reads the poison and fails loudly.
    d.req_valid_i = 0;
    d.req_wf_i = poison_wf(wf);
    d.req_dst_i = poison_dst(dst);
    // The bundle, the mode and the unit selector are captured on the same terms
    // as the tag, so they are poisoned on the same terms too. Without this, a
    // component taken LIVE reads what it read before and the carry is untested.
    d.req_mode_i = 3 - mode;
    d.req_unit_i = 3 - unit_sel;
    // THE IMMEDIATE IS POISONED FOR THE SAME REASON THE TAG IS. It was added to
    // this block after the tag test was written, so nothing drove it and
    // nothing could tell a CARRIED immediate from a LIVE one -- mutant M119
    // survived the whole suite on exactly that. The core cannot catch it
    // either, and that is structural rather than an oversight: the long-op
    // interlock keeps one request in flight, so the core's lq_imm sits stable
    // while the serialiser works and live equals captured by construction.
    d.req_imm_i = ~imm;
    for (int l = 0; l < kLanes; ++l) {
      d.req_a_i[l] = 0xDEAD0000u;
      d.req_a1_i[l] = 0xDEAD0001u;
      d.req_a2_i[l] = 0xDEAD0002u;
      d.req_b0_i[l] = 0xDEAD0003u;
      d.req_b1_i[l] = 0xDEAD0004u;
    }
    drive_unit();
    while (!d.rsp_valid_o && clocks < guard) {
      step();
      ++clocks;
    }
    for (int l = 0; l < kLanes; ++l) y[l] = static_cast<int32_t>(d.rsp_y_o[l]);
    return clocks;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v2_lanemux dut;

  // ---- 1. lane order and the model's arithmetic ---------------------------
  // Distinct values per lane, so an off-by-one in the collect index shows up.
  {
    Bench b(dut, 5);
    const int32_t a[kLanes] = {11, 22, 33, 44};
    int32_t y[kLanes] = {};
    b.transact(3, 17, 1, a, y);
    check(dut.rsp_wf_o == 3, "1.the reply carries the wavefront that asked", 3, dut.rsp_wf_o);
    check(dut.rsp_dst_o == 17, "1.the reply carries the destination that asked", 17, dut.rsp_dst_o);
    uint64_t bad = 0;
    for (int l = 0; l < kLanes; ++l)
      if (y[l] != ScalarUnit::f(a[l])) ++bad;
    check(bad == 0, "1.every lane's answer lands in that lane's slot", 0, bad);
  }

  // ---- 2. TAG CONSERVATION across many different tags ---------------------
  // The property v1 never needed. A tag recomputed from front-end state instead
  // of carried would pass a single transaction and fail here.
  {
    Bench b(dut, 3);
    uint64_t bad_tag = 0, bad_val = 0;
    for (int t = 0; t < 32; ++t) {
      const int wf = t % 8;
      const int dst = (t * 7) % 64;
      int32_t a[kLanes];
      for (int l = 0; l < kLanes; ++l) a[l] = t * 100 + l;
      int32_t y[kLanes] = {};
      b.transact(wf, dst, t % 3, a, y);
      if (dut.rsp_wf_o != wf || dut.rsp_dst_o != dst) ++bad_tag;
      for (int l = 0; l < kLanes; ++l)
        if (y[l] != ScalarUnit::f(a[l])) ++bad_val;
    }
    check(bad_tag == 0, "2.every reply carries back its own wavefront and destination", 0, bad_tag);
    check(bad_val == 0, "2.and every lane's value survives the round trip", 0, bad_val);
  }

  // ---- 3. no reply invented, none lost ------------------------------------
  {
    Bench b(dut, 4);
    const int kN = 16;
    for (int t = 0; t < kN; ++t) {
      int32_t a[kLanes] = {t, t + 1, t + 2, t + 3};
      int32_t y[kLanes] = {};
      b.transact(t % 8, t, 0, a, y);
    }
    check(b.unit.accepted == static_cast<uint64_t>(kN * kLanes),
          "3.the scalar unit saw exactly LANES requests per vector instruction", kN * kLanes,
          b.unit.accepted);
    check(b.unit.replied == b.unit.accepted, "3.every scalar request got exactly one reply",
          b.unit.accepted, b.unit.replied);
  }

  // ---- 4. THE SERIALISATION COST, measured --------------------------------
  // A shared scalar unit serving LANES lanes costs LANES x II per vector
  // instruction. This is the number that puts CURVE first in the v2 work order,
  // so it is reported rather than argued.
  for (int lat : {1, 5, 23}) {
    Bench b(dut, lat);
    const int32_t a[kLanes] = {1, 2, 3, 4};
    int32_t y[kLanes] = {};
    const int clocks = b.transact(0, 0, 0, a, y);
    std::printf("  scalar unit II=%2d -> vector instruction costs %3d clocks (%d lanes)\n", lat,
                clocks, kLanes);
    check(clocks >= lat * kLanes,
          "4.a vector long op costs at least LANES x II, as the model assumes", 1,
          (clocks >= lat * kLanes) ? 1 : 0);
  }

  // ---- 5. THE OPERAND BUNDLE AND THE UNIT SELECTOR ------------------------
  // CURVE takes one operand; zhao_field_len's DIST2 takes five (a0,a1,a2,b0,b1).
  // So the request is a bundle, and the unit selector rides with it -- this
  // block stays unit-agnostic and the core routes on what comes back out.
  //
  // Every component of every lane is a different number, and all of them are
  // POISONED after the accept. A component sourced from the live input rather
  // than the captured copy therefore reads 0xDEADxxxx and fails loudly, which
  // is the same trap that caught M85/M86 on the tag.
  {
    Bench b(dut, 3);
    const int32_t a[kLanes] = {101, 202, 303, 404};
    int32_t y[kLanes] = {};
    b.transact(2, 9, 2, a, y, 4096, /*unit_sel=*/1, /*imm=*/0xA5C3F00Du);

    check(b.obs.size() == static_cast<size_t>(kLanes),
          "5.the unit saw exactly one request per lane", kLanes, b.obs.size());

    uint64_t bad_comp = 0, bad_mode = 0, bad_unit = 0;
    for (size_t k = 0; k < b.obs.size() && k < kLanes; ++k) {
      const Bench::Obs& o = b.obs[k];
      if (o.a != a[k] || o.a1 != a[k] + 1000 || o.a2 != a[k] + 2000 || o.b0 != a[k] + 3000 ||
          o.b1 != a[k] + 4000)
        ++bad_comp;
      if (o.mode != 2) ++bad_mode;
      if (o.unit != 1) ++bad_unit;
    }
    check(bad_comp == 0, "5.every component of every lane arrives intact", 0, bad_comp);
    check(bad_mode == 0, "5.the mode is the CAPTURED one on every lane", 0, bad_mode);
    check(bad_unit == 0, "5.the unit selector is the CAPTURED one on every lane", 0, bad_unit);

    // The immediate is a hash SEED for RIDGE/NOISE2 and an AXIS SELECT for
    // ROT3. Both fail QUIETLY when wrong: different noise, or the same world
    // rotated about the wrong axis. So it is checked on every lane against the
    // value that was requested, with the request line holding its complement.
    uint64_t bad_imm = 0;
    for (size_t k = 0; k < b.obs.size() && k < kLanes; ++k)
      if (b.obs[k].imm != 0xA5C3F00Du) ++bad_imm;
    check(bad_imm == 0, "5.the immediate is the CAPTURED one on every lane", 0, bad_imm);
  }

  dut.final();
  return zhao::report_and_exit("field_v2_lanemux_directed");
}
