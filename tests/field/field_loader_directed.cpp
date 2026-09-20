// field_loader_directed.cpp -- THE FH2 TRANSACTIONAL LOADER.
//
// Owner decisions FH13 (reserve -> fill -> validate -> seal -> bind), FH15
// (backing storage is not the active cache) and FH16 (pin at association
// open), against the install FSM of directive section 10.3.
//
// THE BYTES ARE REAL AND THEY ARRIVE THE REAL WAY. Every capsule this file
// installs is produced by the production packer library --
// `zfield::host_plan::lower` + `serialize_program_image` over the committed
// `crater_ring` program -- and enters the DUT only through the
// `zhao_hps_burst_req_t` bridge port that `zhao_terrain_pageloader` uses.
// Directive 20.5 is explicit that this is the point: "Exercise it with the
// production packer's bytes through the real bridge/arbiter shape. ... A
// testbench secretly writing the engine's private uop/table/scalar ports is
// not that path." There is no back door in this file; the harness plays an HPS
// memory and the loader must ask it for every byte.
//
// THE CASES, and each malformed capsule is derived from the VALID one by a
// single named mutation, so the case that discriminates is nameable:
//
//   FT049  a real PROGRAM capsule installs and becomes READY
//   FT050  version / reserved / count / offset / alignment -- each with an
//          invalid control that fails BEFORE a runnable object is published
//   FT051  extent, address-add overflow and an HPS address above the
//          reachable range refuse WITHOUT truncation or writes
//   FT052  early burst last, missing beat, extra beat -- each a declared
//          counted failure, never stale RAM accepted as payload
//   FT053  a bridge refusal without grant does not hang the client
//   FT056  an interrupted install leaves NO READY object, and a subsequent
//          valid install succeeds without inheriting stale completeness bits
//   FT057  the loader targets the ALLOCATOR's reserved slot, and an eviction
//          selection is forced that differs from the software hint
//   FT060b an unknown FH2 kind is refused and writes nothing
//   FT065  a full, fully PINNED catalogue returns NO_CAPACITY promptly
//
// EVERY ASSERTION IS ON THE CORRECT BEHAVIOUR. Nothing here passes only while
// a defect exists (CLAUDE.md: "do not write a test that asserts the bug").
//
// ENFORCED-BY: itself, as the `field_loader_directed` ctest.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_field_loader.h"

#include "crater_ring.hpp"
#include "zfield/generated/zfield_host_image.hpp"
#include "zfield/zfield.hpp"
#include "zfield/zfield_host_plan.hpp"
#include "zfield/zfield_plan.hpp"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
namespace hp = zfield::host_plan;
namespace hi = zfield::host_image;

using Dut = Vtb_field_loader;

// The loader's verdict codes, mirrored. A test that reads a verdict as a bare
// integer cannot say which refusal it got, and "the install failed" is not a
// diagnosis.
constexpr uint8_t kVOk = 0;
constexpr uint8_t kVBadEnvelope = 1;
constexpr uint8_t kVBadRange = 2;
constexpr uint8_t kVBadSection = 3;
constexpr uint8_t kVBadCrc = 4;
constexpr uint8_t kVBadMeta = 5;
constexpr uint8_t kVBridgeErr = 6;
constexpr uint8_t kVNoCapacity = 7;
constexpr uint8_t kVBadOperation = 8;
constexpr uint8_t kVBadBinding = 9;

constexpr uint8_t kKInstall = 0;
constexpr uint8_t kKBind = 1;
constexpr uint8_t kKControl = 2;
constexpr uint8_t kKReserved = 3;

constexpr uint8_t kVerbOpen = 1;
constexpr uint8_t kVerbClose = 2;
constexpr uint8_t kVerbRelease = 4;
constexpr uint8_t kVerbQuery = 5;

// The staging region the loader is configured to accept, and the live epoch.
constexpr uint32_t kStageBase = 0x1000'0000u;
constexpr uint32_t kStageBytes = 0x0010'0000u;
constexpr uint32_t kEpoch = 0x0000'2026u;
constexpr uint32_t kPlan = 0xF1E1'D000u;

// MEASURED, not assumed: crater_ring's LOWERED out_base is 3, not R111's
// canonical 13. `field_host_plan_directed.cpp:461-465` records the distinction
// and using the canonical number here refuses with kRefusalOutsideWindow.
constexpr int kCraterLoweredOutBase = 3;

void step(Dut& d) { zhao::tick(d); }

// ---------------------------------------------------------------------------
// THE PLAYED BRIDGE LIVES IN `tb_field_loader.sv`
// ---------------------------------------------------------------------------
// The C++ side does not construct beats, does not touch a packed struct, and
// has no path to the loader's catalogue. It loads the played HPS memory a
// 64-bit word at a time through `mw_*` and then sets fault knobs. `kNever`
// disables a fault; anything else names the burst index it applies to.
constexpr uint8_t kNever = 0xFF;

struct Faults {
  bool withhold_grant = false;
  uint8_t err_burst = kNever;
  uint8_t early_last_burst = kNever;
  uint8_t drop_beat_burst = kNever;
  uint8_t extra_beat_burst = kNever;
};

struct Reply {
  bool seen = false;
  bool ok = false;
  uint8_t verdict = 0xFF;
  uint32_t ticket = 0;
  uint32_t plan = 0;
  uint32_t handle = 0;
  uint8_t slot = 0;
  bool evicted = false;
};

void set_faults(Dut& d, const Faults& f) {
  d.cfg_withhold_grant_i = f.withhold_grant ? 1 : 0;
  d.cfg_err_burst_i = f.err_burst;
  d.cfg_early_last_burst_i = f.early_last_burst;
  d.cfg_drop_beat_burst_i = f.drop_beat_burst;
  d.cfg_extra_beat_burst_i = f.extra_beat_burst;
}

// Spin the clock and capture the terminal reply if one arrives.
Reply run(Dut& d, int budget) {
  Reply rep;
  d.fh2_resp_ready_i = 1;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    if (d.fh2_resp_valid_o && !rep.seen) {
      rep.seen = true;
      rep.ok = d.fh2_resp_ok_o != 0;
      rep.verdict = d.fh2_resp_verdict_o;
      rep.ticket = d.fh2_resp_ticket_o;
      rep.plan = d.fh2_resp_plan_o;
      rep.handle = d.fh2_resp_handle_o;
      rep.slot = d.fh2_resp_slot_o;
      rep.evicted = d.fh2_resp_evicted_o != 0;
    }
    step(d);
  }
  return rep;
}

void reset(Dut& d) {
  d.rst_n = 0;
  d.mw_en = 0;
  d.mw_addr = 0;
  d.mw_data = 0;
  d.cfg_hps_window_base_i = kStageBase;
  d.cfg_epoch_i = kEpoch;
  d.cfg_stage_base_i = kStageBase;
  d.cfg_stage_bytes_i = kStageBytes;
  d.fh2_valid_i = 0;
  d.fh2_kind_i = 0;
  d.fh2_verb_i = 0;
  d.fh2_data0_i = 0;
  d.fh2_data1_i = 0;
  d.fh2_data2_i = 0;
  d.fh2_hash_i = 0;
  d.fh2_ticket_i = 0;
  d.fh2_plan_i = kPlan;
  d.fh2_resp_ready_i = 0;
  d.pub_sel_i = 0;
  Faults none;
  set_faults(d, none);
  for (int i = 0; i < 6; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

// Fill the played HPS memory with the capsule, a 64-bit word at a time. This
// is the ONLY way bytes enter the device under test.
void load_mem(Dut& d, const std::vector<uint8_t>& bytes, uint32_t base) {
  d.cfg_hps_window_base_i = base;
  const size_t words = (bytes.size() + 7) / 8;
  for (size_t w = 0; w < words; ++w) {
    uint64_t v = 0;
    for (int b = 0; b < 8; ++b) {
      const size_t idx = w * 8 + static_cast<size_t>(b);
      const uint8_t byte = (idx < bytes.size()) ? bytes[idx] : 0xA5;
      v |= static_cast<uint64_t>(byte) << (8 * b);
    }
    d.mw_en = 1;
    d.mw_addr = static_cast<uint16_t>(w);
    d.mw_data = v;
    step(d);
  }
  d.mw_en = 0;
  step(d);
}

// Offer one FH2 command and wait for the loader to take it.
bool offer(Dut& d, uint8_t kind, uint8_t verb, uint32_t d0, uint32_t d1, uint32_t d2,
           uint32_t hash, uint32_t ticket) {
  d.fh2_valid_i = 1;
  d.fh2_kind_i = kind;
  d.fh2_verb_i = verb;
  d.fh2_data0_i = d0;
  d.fh2_data1_i = d1;
  d.fh2_data2_i = d2;
  d.fh2_hash_i = hash;
  d.fh2_ticket_i = ticket;
  d.fh2_plan_i = kPlan;
  for (int g = 0; g < 64; ++g) {
    d.eval();
    if (d.fh2_ready_o) {
      step(d);
      d.fh2_valid_i = 0;
      return true;
    }
    step(d);
  }
  d.fh2_valid_i = 0;
  return false;
}

// THE BINDING HANDLE, reconstructed the way the loader issues it:
// {generation[23:16], index[7:0]}. `pub_handle_o` is a DIFFERENT number -- it
// is the capsule's CANONICAL PROGRAM HANDLE, which is what BIND matches
// against, not what OPEN names. Passing one where the other belongs is exactly
// the confusion FH16 is about ("raw cache-slot numbers are not a game-facing
// binding"), and it cost this file a debugging pass: every OPEN silently
// refused and FT065 then found capacity it should not have had.
uint32_t binding_handle_at(Dut& d, int idx) {
  d.pub_sel_i = static_cast<uint8_t>(idx);
  d.eval();
  return (static_cast<uint32_t>(d.pub_gen_o) << 16) | static_cast<uint32_t>(idx);
}

// The capsule's canonical program handle for slot `idx`.
uint32_t pub_handle_at(Dut& d, int idx) {
  d.pub_sel_i = static_cast<uint8_t>(idx);
  d.eval();
  return d.pub_handle_o;
}

// ---------------------------------------------------------------------------
// THE REAL CAPSULE
// ---------------------------------------------------------------------------
struct Capsule {
  std::vector<uint8_t> bytes;
  uint32_t crc = 0;   // CRC32C with bytes 12..15 zeroed -- the packer's rule
  bool ok = false;
};

hp::LowerOptions crater_opts() {
  hp::LowerOptions o;
  o.scalar_base = hp::kDefaultScalarBase;
  o.out_base = kCraterLoweredOutBase;
  o.out_lanes = 7;  // the composed OUT_LANES
  o.register_ceiling = hp::kPhysicalRegisters;
  o.resource_epoch = kEpoch;
  return o;
}

Capsule build_capsule(uint32_t handle32) {
  Capsule c;
  zfield::DecodeResult dr = zfield::decode(zfield_gen::crater_ring::kProgramBytes.data(),
                                           zfield_gen::crater_ring::kProgramBytesLen);
  if (dr.error != zfield::DecodeError::kOk) {
    std::printf("  FAIL crater_ring did not decode: %s\n",
                zfield::decodeErrorName(dr.error));
    return c;
  }
  const zfield::Fplan fp = zfield::plan(dr.prog, 0x3);  // Earth: x and z vary
  std::vector<int32_t> in(dr.prog.in_lanes.size(), 0);
  const zfield::Prepared prep = zfield::prepare(fp, dr.prog, in.data(), in.size());

  hp::LowerOptions opt = crater_opts();
  opt.canonical_program_handle32 = handle32;
  opt.source_id32 = dr.prog.source_id;

  hp::HostPlan p;
  std::string refusal;
  if (!hp::lower(fp, dr.prog, &prep, opt, &p, &refusal)) {
    std::printf("  FAIL lower refused: %s\n", refusal.c_str());
    return c;
  }
  if (!hp::serialize_program_image(p, zfield_gen::crater_ring::kProgramBytes.data(),
                                   zfield_gen::crater_ring::kProgramBytesLen, &c.bytes,
                                   &refusal)) {
    std::printf("  FAIL serialize refused: %s\n", refusal.c_str());
    return c;
  }
  // Reproduce the packer's own rule rather than reading the stored field, so
  // the number this test hands the hardware is one it computed itself.
  std::vector<uint8_t> z = c.bytes;
  for (int i = 0; i < 4; ++i) z[hi::ZFH_HDR_OFF_BODY_CRC32C + i] = 0;
  c.crc = hp::crc32c(z.data(), z.size());
  c.ok = true;
  return c;
}

uint32_t rd32(const std::vector<uint8_t>& v, size_t off) {
  uint32_t x = 0;
  std::memcpy(&x, &v[off], 4);
  return x;
}
void wr32(std::vector<uint8_t>& v, size_t off, uint32_t x) {
  std::memcpy(&v[off], &x, 4);
}
void wr16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
  std::memcpy(&v[off], &x, 2);
}

// Re-stamp body_crc32c after a mutation, so that a case testing (say) a bad
// VERSION is not accidentally also testing a bad CRC. Without this every
// malformed-capsule case would refuse for the same reason and the per-class
// verdicts would be untested.
uint32_t restamp(std::vector<uint8_t>& v) {
  std::vector<uint8_t> z = v;
  for (int i = 0; i < 4; ++i) z[hi::ZFH_HDR_OFF_BODY_CRC32C + i] = 0;
  const uint32_t crc = hp::crc32c(z.data(), z.size());
  wr32(v, hi::ZFH_HDR_OFF_BODY_CRC32C, crc);
  return crc;
}

// Install `cap` at `addr` and return the reply.
Reply install(Dut& d, const std::vector<uint8_t>& cap, uint32_t crc, uint32_t addr,
              uint32_t ticket, int budget = 4000) {
  load_mem(d, cap, addr);
  offer(d, kKInstall, 0, addr, 0u, static_cast<uint32_t>(cap.size()), crc, ticket);
  return run(d, budget);
}

int g_ready_bits(Dut& d) {
  int n = 0;
  for (int i = 0; i < 8; ++i) {
    if ((d.pub_ready_o >> i) & 1) ++n;
  }
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut);


  // =========================================================================
  // FT049. A REAL PROGRAM CAPSULE INSTALLS AND BECOMES READY.
  // =========================================================================
  const uint32_t kHandle = 0x484A'DD8Du;  // crater_ring's canonical hash
  Capsule cap = build_capsule(kHandle);
  check(cap.ok, "the production packer produced a capsule", 1, cap.ok ? 1 : 0);
  if (!cap.ok) return zhao::report_and_exit("field_loader_directed");

  std::printf("field_loader_directed: capsule %zu bytes, crc32c=%08X, sections=%u\n",
              cap.bytes.size(), cap.crc,
              static_cast<unsigned>(cap.bytes[hi::ZFH_HDR_OFF_SECTION_COUNT]));

  check((cap.bytes.size() % 64) == 0, "the capsule is 64-byte aligned", 0,
        static_cast<int>(cap.bytes.size() % 64));
  check(rd32(cap.bytes, hi::ZFH_HDR_OFF_TOTAL_BYTES) == cap.bytes.size(),
        "total_bytes agrees with the serialized length",
        static_cast<int>(cap.bytes.size()),
        static_cast<int>(rd32(cap.bytes, hi::ZFH_HDR_OFF_TOTAL_BYTES)));

  {
    Reply r = install(dut, cap.bytes, cap.crc, kStageBase, 0x1001);
    check(r.seen, "FT049: the install was ANSWERED", 1, r.seen ? 1 : 0);
    check(r.ok, "FT049: the install SUCCEEDED", 1, r.ok ? 1 : 0);
    check(r.verdict == kVOk, "FT049: verdict is OK", kVOk, r.verdict);
    check(r.ticket == 0x1001u, "FT049: the reply carried the caller's own ticket",
          0x1001, static_cast<int>(r.ticket));
    check(r.plan == kPlan, "FT049: and the plan it was posted under",
          static_cast<int>(kPlan), static_cast<int>(r.plan));
    check(dut.installs_ok_o == 1, "FT049: installs_ok_o FIRED", 1,
          static_cast<int>(dut.installs_ok_o));
    check(g_ready_bits(dut) == 1, "FT049: exactly one object is READY", 1,
          g_ready_bits(dut));
    check((dut.pub_ready_o & 1u) != 0, "FT049: and it is the allocator's slot 0", 1,
          (dut.pub_ready_o & 1u) ? 1 : 0);
    check(dut.load_bytes_o >= cap.bytes.size(),
          "FT049: every byte of the capsule came through the BRIDGE", 1,
          (dut.load_bytes_o >= cap.bytes.size()) ? 1 : 0);
    // The handle is a generation-bearing binding, not a raw slot number.
    check(r.handle != 0, "FT049: a binding handle was issued", 1, r.handle ? 1 : 0);
    check((r.handle >> 16) != 0, "FT049: and it carries a GENERATION", 1,
          ((r.handle >> 16) != 0) ? 1 : 0);
  }

  // =========================================================================
  // FT050. EVERY VERSION / RESERVED / COUNT / ALIGNMENT CHECK HAS AN INVALID
  // CONTROL THAT FAILS BEFORE A RUNNABLE OBJECT IS PUBLISHED.
  // =========================================================================
  // Each control is the VALID capsule with ONE named field changed and the
  // body CRC re-stamped, so the case that discriminates is the field itself
  // and not an incidental checksum mismatch.
  struct Bad {
    const char* name;
    size_t off;
    int width;      // 2 or 4, or 1
    uint32_t value;
    uint8_t want;
  };
  const Bad bads[] = {
      {"image_version", hi::ZFH_HDR_OFF_IMAGE_VERSION, 2, 99, kVBadEnvelope},
      {"host_protocol_version", hi::ZFH_HDR_OFF_HOST_PROTOCOL_VERSION, 2, 7,
       kVBadEnvelope},
      {"header_bytes", hi::ZFH_HDR_OFF_HEADER_BYTES, 2, 32, kVBadEnvelope},
      {"header flags (reserved, must be zero)", hi::ZFH_HDR_OFF_FLAGS, 1, 0x40,
       kVBadEnvelope},
      {"reserved_18", hi::ZFH_HDR_OFF_RESERVED_18, 2, 0x1234, kVBadEnvelope},
      {"reserved_60", hi::ZFH_HDR_OFF_RESERVED_60, 4, 0xDEADBEEF, kVBadEnvelope},
      {"object_kind out of range", hi::ZFH_HDR_OFF_OBJECT_KIND, 1, 9, kVBadEnvelope},
      {"section_count = 0", hi::ZFH_HDR_OFF_SECTION_COUNT, 2, 0, kVBadSection},
      {"section_count absurd", hi::ZFH_HDR_OFF_SECTION_COUNT, 2, 4000, kVBadSection},
      {"resource_epoch (stale capsule)", hi::ZFH_HDR_OFF_RESOURCE_EPOCH, 4, 0x999,
       kVBadMeta},
      {"canonical_program_handle32 = 0", hi::ZFH_HDR_OFF_CANONICAL_PROGRAM_HANDLE32, 4,
       0, kVBadMeta},
  };

  // Read the PER-CLASS counter that the expected verdict names. Asserting the
  // verdict alone would show the check fired; asserting the counter shows the
  // INSTRUMENT for that class can see it. Rule: every new counter owes a
  // demonstration that it can SEE the thing, and a per-class counter nobody
  // watches move is the shape this console keeps finding.
  auto counter_for = [&dut](uint8_t verdict) -> uint32_t {
    switch (verdict) {
      case kVBadEnvelope: return dut.bad_envelope_o;
      case kVBadRange:    return dut.bad_range_o;
      case kVBadSection:  return dut.bad_section_o;
      case kVBadCrc:      return dut.bad_crc_o;
      case kVBadMeta:     return dut.bad_meta_o;
      case kVBridgeErr:   return dut.bridge_errs_o;
      case kVNoCapacity:  return dut.no_capacity_o;
      case kVBadOperation:return dut.bad_operation_o;
      default:            return 0u;
    }
  };

  for (const Bad& b : bads) {
    const uint32_t ready_before = dut.pub_ready_o;
    const uint32_t ok_before = dut.installs_ok_o;
    const uint32_t class_before = counter_for(b.want);
    const uint32_t failed_before = dut.installs_failed_o;
    std::vector<uint8_t> v = cap.bytes;
    if (b.width == 1) {
      v[b.off] = static_cast<uint8_t>(b.value);
    } else if (b.width == 2) {
      wr16(v, b.off, static_cast<uint16_t>(b.value));
    } else {
      wr32(v, b.off, b.value);
    }
    const uint32_t crc = restamp(v);
    Reply r = install(dut, v, crc, kStageBase, 0x2000);

    char msg[256];
    std::snprintf(msg, sizeof(msg), "FT050 control [%s]: REFUSED", b.name);
    check(r.seen && !r.ok, msg, 1, (r.seen && !r.ok) ? 1 : 0);
    std::snprintf(msg, sizeof(msg), "FT050 control [%s]: with verdict %u", b.name,
                  b.want);
    check(r.verdict == b.want, msg, b.want, r.verdict);
    std::snprintf(msg, sizeof(msg),
                  "FT050 control [%s]: NO new READY object was published", b.name);
    check(dut.pub_ready_o == ready_before, msg, static_cast<int>(ready_before),
          static_cast<int>(dut.pub_ready_o));
    std::snprintf(msg, sizeof(msg), "FT050 control [%s]: installs_ok_o did not move",
                  b.name);
    check(dut.installs_ok_o == ok_before, msg, static_cast<int>(ok_before),
          static_cast<int>(dut.installs_ok_o));
    std::snprintf(msg, sizeof(msg),
                  "FT050 control [%s]: its PER-CLASS counter FIRED", b.name);
    check(counter_for(b.want) == class_before + 1, msg,
          static_cast<int>(class_before + 1),
          static_cast<int>(counter_for(b.want)));
    std::snprintf(msg, sizeof(msg), "FT050 control [%s]: installs_failed_o FIRED",
                  b.name);
    check(dut.installs_failed_o == failed_before + 1, msg,
          static_cast<int>(failed_before + 1),
          static_cast<int>(dut.installs_failed_o));
  }

  // The CRC control, kept separate because it is the one check the others
  // deliberately neutralise by re-stamping.
  {
    const uint32_t ready_before = dut.pub_ready_o;
    Reply r = install(dut, cap.bytes, cap.crc ^ 0x1u, kStageBase, 0x2100);
    check(r.seen && !r.ok, "FT050 control [body_crc32c]: REFUSED", 1,
          (r.seen && !r.ok) ? 1 : 0);
    check(r.verdict == kVBadCrc, "FT050 control [body_crc32c]: verdict BAD_CRC",
          kVBadCrc, r.verdict);
    check(dut.bad_crc_o >= 1, "FT050: bad_crc_o FIRED", 1,
          (dut.bad_crc_o >= 1) ? 1 : 0);
    check(dut.pub_ready_o == ready_before,
          "FT050 control [body_crc32c]: NOTHING was published -- the CRC is "
          "checked BEFORE the object is sealed, not after",
          static_cast<int>(ready_before), static_cast<int>(dut.pub_ready_o));
  }

  // =========================================================================
  // FT051. EXTENT, ADDRESS OVERFLOW AND AN UNREACHABLE ADDRESS REFUSE WITHOUT
  // TRUNCATION.
  // =========================================================================
  {
    const uint32_t ready_before = dut.pub_ready_o;

    // (a) a length that is not 64-byte aligned
    load_mem(dut, cap.bytes, kStageBase);
    offer(dut, kKInstall, 0, kStageBase, 0, static_cast<uint32_t>(cap.bytes.size() - 8),
          cap.crc, 0x3001);
    Reply ra = run(dut, 4000);
    check(ra.seen && ra.verdict == kVBadEnvelope,
          "FT051(a): an unaligned byte length is BAD_ENVELOPE", kVBadEnvelope,
          ra.verdict);

    // (b) zero length
    offer(dut, kKInstall, 0, kStageBase, 0, 0, cap.crc, 0x3002);
    Reply rb = run(dut, 4000);
    check(rb.seen && rb.verdict == kVBadEnvelope,
          "FT051(b): a zero byte length is BAD_ENVELOPE", kVBadEnvelope, rb.verdict);

    // (c) AN ADDRESS ABOVE THE REACHABLE RANGE. The high 32 bits are non-zero,
    //     so a loader that truncated would read a perfectly plausible low
    //     address and install stale RAM under a valid-looking CRC. This is the
    //     case that must refuse rather than narrow.
    offer(dut, kKInstall, 0, kStageBase, /*d1 = addr[63:32]*/ 0x0000'0001u,
          static_cast<uint32_t>(cap.bytes.size()), cap.crc, 0x3003);
    Reply rc = run(dut, 4000);
    check(rc.seen && rc.verdict == kVBadRange,
          "FT051(c): an address above the reachable range is BAD_RANGE, "
          "NOT truncated to its low word",
          kVBadRange, rc.verdict);
    check(dut.bad_range_o >= 1, "FT051: bad_range_o FIRED", 1,
          (dut.bad_range_o >= 1) ? 1 : 0);

    // (d) addr + len leaves the staging region
    offer(dut, kKInstall, 0, kStageBase + kStageBytes - 64, 0,
          static_cast<uint32_t>(cap.bytes.size()), cap.crc, 0x3004);
    Reply rd_ = run(dut, 4000);
    check(rd_.seen && rd_.verdict == kVBadRange,
          "FT051(d): addr+len past the staging region is BAD_RANGE", kVBadRange,
          rd_.verdict);

    // (e) an address BELOW the region
    offer(dut, kKInstall, 0, kStageBase - 64, 0,
          static_cast<uint32_t>(cap.bytes.size()), cap.crc, 0x3005);
    Reply re = run(dut, 4000);
    check(re.seen && re.verdict == kVBadRange,
          "FT051(e): an address below the staging region is BAD_RANGE", kVBadRange,
          re.verdict);

    check(dut.pub_ready_o == ready_before,
          "FT051: and none of the five published anything",
          static_cast<int>(ready_before), static_cast<int>(dut.pub_ready_o));

    // THE NEGATIVE CONTROL. Without it, a loader that refused every address
    // would score full marks above.
    Reply rok = install(dut, cap.bytes, cap.crc, kStageBase + 64, 0x3006);
    check(rok.seen && rok.ok,
          "FT051 NEGATIVE CONTROL: a legal in-region address still installs", 1,
          (rok.seen && rok.ok) ? 1 : 0);
  }

  // =========================================================================
  // FT052. BRIDGE FAULTS EACH PRODUCE A DECLARED COUNTED FAILURE, NEVER STALE
  // RAM ACCEPTED AS PAYLOAD.
  // =========================================================================
  {
    struct Fault {
      const char* name;
      uint8_t early_last;
      uint8_t drop_beat;
      uint8_t extra_beat;
      uint8_t err;
    };
    // Burst 1 rather than burst 0: burst 0 is the HEADER read, and a fault
    // there is caught by a different arm of the FSM. Faulting the BODY is what
    // exercises the path that has already reserved an object, which is the
    // half that must not publish.
    const Fault faults[] = {
        {"early burst last", 1, kNever, kNever, kNever},
        {"missing beat", kNever, 1, kNever, kNever},
        {"extra beat", kNever, kNever, 1, kNever},
        {"bridge err", kNever, kNever, kNever, 1},
    };
    for (const Fault& f : faults) {
      const uint32_t ready_before = dut.pub_ready_o;
      const uint32_t errs_before = dut.bridge_errs_o;
      load_mem(dut, cap.bytes, kStageBase);
      Faults ft;
      ft.early_last_burst = f.early_last;
      ft.drop_beat_burst = f.drop_beat;
      ft.extra_beat_burst = f.extra_beat;
      ft.err_burst = f.err;
      set_faults(dut, ft);

      offer(dut, kKInstall, 0, kStageBase, 0,
            static_cast<uint32_t>(cap.bytes.size()), cap.crc, 0x4000);
      Reply r = run(dut, 6000);

      char msg[256];
      std::snprintf(msg, sizeof(msg), "FT052 [%s]: the install did NOT succeed",
                    f.name);
      check(!(r.seen && r.ok), msg, 1, (r.seen && r.ok) ? 0 : 1);
      std::snprintf(msg, sizeof(msg),
                    "FT052 [%s]: and NO object was published from a broken burst",
                    f.name);
      check(dut.pub_ready_o == ready_before, msg, static_cast<int>(ready_before),
            static_cast<int>(dut.pub_ready_o));
      if (r.seen) {
        std::snprintf(msg, sizeof(msg), "FT052 [%s]: bridge_errs_o moved", f.name);
        check(dut.bridge_errs_o > errs_before, msg, 1,
              (dut.bridge_errs_o > errs_before) ? 1 : 0);
      }
      reset(dut);
    }
  }

  // =========================================================================
  // FT053. A BRIDGE REFUSAL WITHOUT GRANT DOES NOT HANG THE CLIENT.
  // =========================================================================
  {
    load_mem(dut, cap.bytes, kStageBase);
    Faults ft;
    ft.withhold_grant = true;
    set_faults(dut, ft);
    offer(dut, kKInstall, 0, kStageBase, 0, static_cast<uint32_t>(cap.bytes.size()),
          cap.crc, 0x5001);
    Reply r = run(dut, 2000);
    check(!r.seen, "FT053: with no grant the loader is still waiting, as designed", 1,
          r.seen ? 0 : 1);
    // The thing that must be true: when the grant finally arrives, the SAME
    // transaction completes. A client that had given up, or corrupted its
    // state while waiting, would fail here.
    ft.withhold_grant = false;
    set_faults(dut, ft);
    Reply r2 = run(dut, 6000);
    check(r2.seen && r2.ok,
          "FT053: and the held transaction completes once the bridge grants -- "
          "the client neither hung nor lost its request",
          1, (r2.seen && r2.ok) ? 1 : 0);
    check(r2.ticket == 0x5001u, "FT053: with its ORIGINAL ticket", 0x5001,
          static_cast<int>(r2.ticket));
    reset(dut);
  }

  // =========================================================================
  // FT056. AN INTERRUPTED INSTALL LEAVES NO READY OBJECT, AND THE NEXT VALID
  // INSTALL DOES NOT INHERIT STALE COMPLETENESS BITS.
  // =========================================================================
  {
    // A good install first, so there is a prior READY object to protect.
    Reply r0 = install(dut, cap.bytes, cap.crc, kStageBase, 0x6001);
    check(r0.ok, "FT056: a first capsule installed", 1, r0.ok ? 1 : 0);
    const int ready_after_good = g_ready_bits(dut);

    // Now an install that dies mid-body, AFTER a slot has been reserved. This
    // is the case that distinguishes "reserve then publish" from "publish then
    // check": under the wrong order the reserved slot would already be READY
    // by the time the bridge failed.
    load_mem(dut, cap.bytes, kStageBase);
    Faults ft;
    ft.err_burst = 2;
    set_faults(dut, ft);
    offer(dut, kKInstall, 0, kStageBase, 0, static_cast<uint32_t>(cap.bytes.size()),
          cap.crc, 0x6002);
    Reply r1 = run(dut, 6000);
    Faults clear;
    set_faults(dut, clear);
    check(r1.seen && !r1.ok, "FT056: the interrupted install was REFUSED", 1,
          (r1.seen && !r1.ok) ? 1 : 0);
    check(g_ready_bits(dut) == ready_after_good,
          "FT056: no NEW ready object appeared from the interrupted install",
          ready_after_good, g_ready_bits(dut));

    // And a subsequent valid install succeeds cleanly.
    Reply r2 = install(dut, cap.bytes, cap.crc, kStageBase, 0x6003);
    check(r2.seen && r2.ok,
          "FT056: a subsequent valid install SUCCEEDS without inheriting stale "
          "completeness bits",
          1, (r2.seen && r2.ok) ? 1 : 0);
    check(r2.verdict == kVOk, "FT056: and its verdict is clean", kVOk, r2.verdict);
    reset(dut);
  }

  // =========================================================================
  // FT057. THE LOADER TARGETS THE ALLOCATOR'S RESERVED SLOT, AND AN EVICTION
  // SELECTION IS FORCED THAT DIFFERS FROM THE SOFTWARE HINT.
  // =========================================================================
  //
  // Eight objects, installed in order, so slot 0 is the least-recently-used.
  // Slot 0 is then PINNED by opening an association against it. The ninth
  // install must therefore evict something that is NOT slot 0 -- which is
  // exactly "force an eviction selection different from the hint", because
  // plain LRU (and any software guess that modelled it) would pick slot 0.
  {
    std::vector<uint32_t> handles;
    for (int i = 0; i < 8; ++i) {
      Capsule ci = build_capsule(kHandle + static_cast<uint32_t>(i) + 1u);
      check(ci.ok, "FT057: a distinct capsule was packed", 1, ci.ok ? 1 : 0);
      Reply r = install(dut, ci.bytes, ci.crc, kStageBase,
                        0x7000u + static_cast<uint32_t>(i));
      check(r.ok, "FT057: it installed", 1, r.ok ? 1 : 0);
      handles.push_back(r.handle);
    }
    check(g_ready_bits(dut) == 8, "FT057: the catalogue is full and all READY", 8,
          g_ready_bits(dut));

    // PIN slot 0 -- FH16's "acquire and pin at association open".
    offer(dut, kKControl, kVerbOpen, binding_handle_at(dut, 0), 0, 0, 0, 0x7100);
    Reply rp = run(dut, 400);
    check(rp.seen && rp.ok, "FT057: OPEN_ASSOCIATION pinned the LRU object", 1,
          (rp.seen && rp.ok) ? 1 : 0);
    check((dut.pub_pinned_o & 1u) != 0, "FT057: and pub_pinned_o shows it", 1,
          (dut.pub_pinned_o & 1u) ? 1 : 0);

    const uint32_t hint_before = dut.pin_forced_victim_o;
    const uint32_t evict_before = dut.evictions_o;

    Capsule c9 = build_capsule(kHandle + 0x100u);
    Reply r9 = install(dut, c9.bytes, c9.crc, kStageBase, 0x7200);
    check(r9.seen && r9.ok, "FT057: the ninth install succeeded", 1,
          (r9.seen && r9.ok) ? 1 : 0);
    check(dut.evictions_o == evict_before + 1, "FT057: it EVICTED", 1,
          (dut.evictions_o == evict_before + 1) ? 1 : 0);
    // THE DISCRIMINATING ASSERTION. Slot 0 is the least-recently-used and is
    // what any software pre-commit guess would name; it is pinned, so the
    // allocator must have chosen a different victim.
    check(r9.slot != 0,
          "FT057: the allocator chose a victim OTHER than the pinned LRU slot -- "
          "an eviction selection different from the software hint",
          1, (r9.slot != 0) ? 1 : 0);
    check((dut.pub_ready_o & 1u) != 0,
          "FT057: and the PINNED object is still resident and READY", 1,
          (dut.pub_ready_o & 1u) ? 1 : 0);
    // FIRED, not merely "did not go backwards". The first version of this
    // assertion read `>= hint_before`, which is true of a counter that is
    // welded shut -- a check that cannot fail, guarding a counter that (as
    // first written) measured something other than its name. Both halves are
    // fixed: the counter now differences the real victim against a PIN-BLIND
    // LRU, and this asserts it MOVED BY ONE on the eviction a pin forced.
    check(dut.pin_forced_victim_o == hint_before + 1,
          "FT057: pin_forced_victim_o FIRED -- a PIN, not luck, chose the victim",
          static_cast<int>(hint_before + 1),
          static_cast<int>(dut.pin_forced_victim_o));

    // =======================================================================
    // FT065. A FULLY PINNED CATALOGUE RETURNS NO_CAPACITY PROMPTLY.
    // =======================================================================
    // Pin everything that is ready, then try once more.
    for (int i = 0; i < 8; ++i) {
      if (((dut.pub_ready_o >> i) & 1) == 0) continue;
      const uint32_t h = binding_handle_at(dut, i);
      offer(dut, kKControl, kVerbOpen, h, 0, 0, 0, 0x7300u + static_cast<uint32_t>(i));
      Reply rop = run(dut, 300);
      check(rop.seen && rop.ok, "FT065: (setup) each READY object was PINNED", 1,
            (rop.seen && rop.ok) ? 1 : 0);
    }
    check(dut.pub_pinned_o == dut.pub_ready_o,
          "FT065: (setup) every ready object is now pinned",
          static_cast<int>(dut.pub_ready_o), static_cast<int>(dut.pub_pinned_o));
    const uint32_t nocap_before = dut.no_capacity_o;
    Capsule cX = build_capsule(kHandle + 0x200u);
    Reply rX = install(dut, cX.bytes, cX.crc, kStageBase, 0x7400);
    check(rX.seen, "FT065: the install on a full pinned catalogue was ANSWERED", 1,
          rX.seen ? 1 : 0);
    check(rX.verdict == kVNoCapacity, "FT065: with NO_CAPACITY", kVNoCapacity,
          rX.verdict);
    check(dut.no_capacity_o == nocap_before + 1, "FT065: no_capacity_o FIRED", 1,
          (dut.no_capacity_o == nocap_before + 1) ? 1 : 0);

    // A RELEASE queued behind it is still serviceable -- directive 10.3's
    // "must not monopolize the maintenance head".
    const uint32_t h0 = binding_handle_at(dut, 0);
    offer(dut, kKControl, kVerbRelease, h0, 0, 0, 0, 0x7500);
    Reply rr = run(dut, 400);
    check(rr.seen && rr.ok,
          "FT065: and a RELEASE queued behind the refusal is still serviced -- "
          "the drain path stayed available",
          1, (rr.seen && rr.ok) ? 1 : 0);
    reset(dut);
  }

  // =========================================================================
  // FT060b. AN UNKNOWN FH2 KIND, AND AN UNKNOWN CONTROL VERB, ARE REFUSED AND
  // WRITE NOTHING.
  // =========================================================================
  {
    const uint32_t bad_op_before = dut.bad_operation_o;
    offer(dut, kKReserved, 0, 0, 0, 0, 0, 0x8001);
    Reply r = run(dut, 400);
    check(r.seen && !r.ok, "FT060b: the reserved FH2 kind was REFUSED", 1,
          (r.seen && !r.ok) ? 1 : 0);
    check(r.verdict == kVBadOperation, "FT060b: with BAD_OPERATION", kVBadOperation,
          r.verdict);
    check(dut.bad_operation_o == bad_op_before + 1, "FT060b: bad_operation_o FIRED",
          1, (dut.bad_operation_o == bad_op_before + 1) ? 1 : 0);
    check(dut.load_bytes_o == 0,
          "FT060b: and NOT ONE BYTE was read -- an unknown operation does not "
          "touch the bridge",
          0, static_cast<int>(dut.load_bytes_o));
    check(g_ready_bits(dut) == 0, "FT060b: and nothing was published", 0,
          g_ready_bits(dut));

    const uint32_t bad_op2 = dut.bad_operation_o;
    offer(dut, kKControl, /*verb=*/0xEE, 0, 0, 0, 0, 0x8002);
    Reply rv = run(dut, 400);
    check(rv.seen && !rv.ok, "FT060b: an unknown CONTROL verb was REFUSED", 1,
          (rv.seen && !rv.ok) ? 1 : 0);
    check(rv.verdict == kVBadOperation, "FT060b: also BAD_OPERATION", kVBadOperation,
          rv.verdict);
    check(dut.bad_operation_o == bad_op2 + 1,
          "FT060b: and bad_operation_o fired again", 1,
          (dut.bad_operation_o == bad_op2 + 1) ? 1 : 0);

    // NEGATIVE CONTROL: a KNOWN verb is accepted, so the assertions above are
    // not satisfied by a loader that refuses every control.
    reset(dut);
    Reply rg = install(dut, cap.bytes, cap.crc, kStageBase, 0x8003);
    check(rg.ok, "FT060b: (setup) an object to query", 1, rg.ok ? 1 : 0);
    offer(dut, kKControl, kVerbQuery, rg.handle, 0, 0, 0, 0x8004);
    Reply rq = run(dut, 400);
    check(rq.seen && rq.ok,
          "FT060b NEGATIVE CONTROL: a KNOWN verb is accepted", 1,
          (rq.seen && rq.ok) ? 1 : 0);
  }

  // =========================================================================
  // BIND resolves the EXACT object, and equal canonical hash is not enough.
  // =========================================================================
  {
    reset(dut);
    Reply ri = install(dut, cap.bytes, cap.crc, kStageBase, 0x9001);
    check(ri.ok, "BIND: (setup) the capsule installed", 1, ri.ok ? 1 : 0);

    // A correct bind: the handle, the canonical program handle32, the epoch
    // and the full-image CRC must ALL agree.
    const uint32_t full_crc = rd32(cap.bytes, hi::ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C);
    offer(dut, kKBind, 0, kHandle, ri.handle, kEpoch, full_crc, 0x9002);
    Reply rb = run(dut, 400);
    check(rb.seen && rb.ok, "BIND: a fully-matching bind SUCCEEDS", 1,
          (rb.seen && rb.ok) ? 1 : 0);
    check(dut.binds_ok_o == 1, "BIND: binds_ok_o FIRED", 1,
          static_cast<int>(dut.binds_ok_o));

    // Each field wrong in turn -- the controls that make the bind a real
    // identity check rather than a handle lookup.
    struct BindBad {
      const char* name;
      uint32_t prog, handle, epoch, crc;
    };
    const BindBad bb[] = {
        {"wrong canonical handle32", kHandle ^ 1u, ri.handle, kEpoch, full_crc},
        {"wrong epoch", kHandle, ri.handle, kEpoch ^ 0xFFu, full_crc},
        {"wrong full-image CRC", kHandle, ri.handle, kEpoch, full_crc ^ 1u},
        {"wrong generation in the handle", kHandle, ri.handle ^ 0x00010000u, kEpoch,
         full_crc},
    };
    for (const BindBad& b : bb) {
      const uint32_t failed_before = dut.binds_failed_o;
      offer(dut, kKBind, 0, b.prog, b.handle, b.epoch, b.crc, 0x9100);
      Reply r = run(dut, 400);
      char msg[256];
      std::snprintf(msg, sizeof(msg), "BIND control [%s]: REFUSED", b.name);
      check(r.seen && !r.ok, msg, 1, (r.seen && !r.ok) ? 1 : 0);
      std::snprintf(msg, sizeof(msg), "BIND control [%s]: verdict BAD_BINDING",
                    b.name);
      check(r.verdict == kVBadBinding, msg, kVBadBinding, r.verdict);
      std::snprintf(msg, sizeof(msg), "BIND control [%s]: binds_failed_o moved",
                    b.name);
      check(dut.binds_failed_o == failed_before + 1, msg, 1,
            (dut.binds_failed_o == failed_before + 1) ? 1 : 0);
    }
  }

  std::printf(
      "field_loader_directed: installs_ok=%u installs_failed=%u binds_ok=%u "
      "binds_failed=%u bad_op=%u bad_env=%u bad_range=%u bad_sec=%u bad_crc=%u "
      "bad_meta=%u bridge_errs=%u nocap=%u evictions=%u\n",
      static_cast<unsigned>(dut.installs_ok_o),
      static_cast<unsigned>(dut.installs_failed_o),
      static_cast<unsigned>(dut.binds_ok_o),
      static_cast<unsigned>(dut.binds_failed_o),
      static_cast<unsigned>(dut.bad_operation_o),
      static_cast<unsigned>(dut.bad_envelope_o),
      static_cast<unsigned>(dut.bad_range_o),
      static_cast<unsigned>(dut.bad_section_o),
      static_cast<unsigned>(dut.bad_crc_o), static_cast<unsigned>(dut.bad_meta_o),
      static_cast<unsigned>(dut.bridge_errs_o),
      static_cast<unsigned>(dut.no_capacity_o),
      static_cast<unsigned>(dut.evictions_o));

  return zhao::report_and_exit("field_loader_directed");
}
