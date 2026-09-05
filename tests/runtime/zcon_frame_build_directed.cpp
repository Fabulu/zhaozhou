// zcon_frame_build_directed.cpp — the console runtime's command construction.
// Authored 2026-09-05 (software lane).
//
// The runtime's stated job includes "command construction". This pins that the
// packets it builds are REAL: sealed with the machinery the capture tools and
// the RTL shell already agree with, and accepted by the console's own
// validator -- `zref::cmd::validate`, which is CMD.DECODER's reference.
//
// The strongest check here is the negative one. A builder that produced
// plausible-looking bytes nothing validated would be worse than no builder,
// because it would fail later, on hardware, as a decode error nobody could
// trace back to construction.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zcon/frame_build.hpp"

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

zcon::DrawItem item(uint32_t i) {
  zcon::DrawItem d;
  d.form = {i, 1, zcon::ResourceKind::kMeshStream};
  d.material_set = {i + 100, 2, zcon::ResourceKind::kMaterialSet};
  d.transform = {i + 200, 3, zcon::ResourceKind::kMeshStream};
  return d;
}

void test_an_empty_frame_validates() {
  zcon::FramePlan p;
  p.frame_id = 1;
  p.sequence = 1;
  p.resource_epoch = 1;
  const zcon::BuiltFrame f = zcon::build_and_validate(p);
  check(f.ok, "a frame with no draws still seals and validates", 0,
        static_cast<long long>(f.verdict.error));
  check(f.bytes.size() > 0, "and has bytes", 1, f.bytes.size() > 0 ? 1 : 0);
}

void test_draws_validate_and_are_counted() {
  for (int n : {1, 2, 8, 64}) {
    zcon::FramePlan p;
    p.frame_id = 2;
    p.sequence = 2;
    p.resource_epoch = 1;
    for (int i = 0; i < n; ++i) p.draws.push_back(item(static_cast<uint32_t>(i)));
    const zcon::BuiltFrame f = zcon::build_and_validate(p);
    check(f.ok, "a frame of draws validates", 0, static_cast<long long>(f.verdict.error));
    // BEGIN + n draws + END
    check(f.verdict.commands_consumed == static_cast<uint32_t>(n + 2),
          "every record was walked by the validator", n + 2,
          static_cast<long long>(f.verdict.commands_consumed));
  }
}

void test_the_header_reports_what_was_built() {
  zcon::FramePlan p;
  p.frame_id = 0x1234;
  p.sequence = 0x99;
  p.resource_epoch = 0x7;
  for (int i = 0; i < 5; ++i) p.draws.push_back(item(static_cast<uint32_t>(i)));
  const zcon::BuiltFrame f = zcon::build_and_validate(p);
  check(f.ok, "built frame validates", 0, static_cast<long long>(f.verdict.error));

  // parse_header is only trustworthy on a VALIDATED packet -- which is why the
  // check above comes first rather than beside it.
  const zref::cmd::Header h = zref::cmd::parse_header(f.bytes.data());
  check(h.frame_id == 0x1234, "frame_id survives the seal", 0x1234, h.frame_id);
  check(h.sequence == 0x99, "sequence survives", 0x99, h.sequence);
  check(h.resource_epoch == 0x7, "resource epoch survives", 0x7, h.resource_epoch);
  check(h.command_count == 7, "command_count is BEGIN + 5 draws + END", 7, h.command_count);
}

void test_handles_reach_the_wire_in_the_abi_layout() {
  // handle32 is {index:24, generation:8}. Pinning it here means a change to the
  // packing is caught in the runtime rather than discovered as a wrong resource
  // being drawn.
  zcon::Handle h;
  h.index = 0xABCDEF;
  h.generation = 0x1234;  // 16-bit in the runtime, low 8 on the wire
  const uint32_t w = zcon::detail::handle32(h);
  check(w == ((0xABCDEFu << 8) | 0x34u),
        "index in the high 24 bits, low byte of the generation beneath",
        static_cast<long long>((0xABCDEFu << 8) | 0x34u), w);

  // The narrowing is a real wire limit, so it is asserted rather than assumed:
  // two handles differing only above bit 8 of the generation are the SAME on
  // the wire. That is worth knowing before it is discovered.
  zcon::Handle a = h, b = h;
  a.generation = 0x0034;
  b.generation = 0x1134;
  check(zcon::detail::handle32(a) == zcon::detail::handle32(b),
        "generations 256 apart collide on the wire -- a stated wire limit", 1,
        zcon::detail::handle32(a) == zcon::detail::handle32(b) ? 1 : 0);
}

void test_a_corrupted_packet_is_refused() {
  zcon::FramePlan p;
  p.frame_id = 3;
  p.sequence = 3;
  p.resource_epoch = 1;
  p.draws.push_back(item(1));
  zcon::BuiltFrame f = zcon::build_and_validate(p);
  check(f.ok, "the packet is good before corruption", 0, static_cast<long long>(f.verdict.error));

  // Flip a byte in the middle of the record stream. The seal's CRCs exist for
  // exactly this, and a builder whose output could be corrupted undetected
  // would make every later decode failure untraceable.
  f.bytes[f.bytes.size() / 2] ^= 0xFF;
  const zref::cmd::Result v = zref::cmd::validate(f.bytes);
  check(v.error != 0, "a single flipped byte is REFUSED by the validator", 1, v.error != 0 ? 1 : 0);
}

}  // namespace

int main() {
  test_an_empty_frame_validates();
  test_draws_validate_and_are_counted();
  test_the_header_reports_what_was_built();
  test_handles_reach_the_wire_in_the_abi_layout();
  test_a_corrupted_packet_is_refused();

  if (g_failed) {
    std::printf("[zcon_frame_build_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[zcon_frame_build_directed] %d checks passed\n", g_checks);
  return 0;
}
