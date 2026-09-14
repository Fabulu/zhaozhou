// texture_uv_join_v2_directed.cpp -- Packet-B joined365 carriage, holds,
// full-owner identity, idle observation, and committed mutant fire controls.
//
// The production result is checked on EVERY stalled cycle, not merely when it
// eventually handshakes.  The A/B control holds joined A while descriptor/UV B
// are accepted behind it.  A must retain all 365 admission-time bits, including
// the sole page generation at logical287[286:279].  Beside it, the two renamed
// mutants must fail in their one intended direction: B-generation/A-owner and
// late-current-active-generation/A-owner respectively.
#include "Vtb_uv_join_v2_pair.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

using Logical = std::array<uint32_t, 9>;
using Joined = std::array<uint32_t, 12>;

constexpr int kLogicalBits = 287;
constexpr int kJoinedBits = 365;
constexpr int kLogicalPageGenerationLsb = 279;
constexpr int kJoinedPageGenerationLsb = 343;

void tick(Vtb_uv_join_v2_pair* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

template <size_t N>
void put_bit(std::array<uint32_t, N>& words, int bit, bool value) {
  const uint32_t mask = uint32_t{1} << (bit & 31);
  if (value)
    words[static_cast<size_t>(bit >> 5)] |= mask;
  else
    words[static_cast<size_t>(bit >> 5)] &= ~mask;
}

template <size_t N>
bool get_bit(const std::array<uint32_t, N>& words, int bit) {
  return ((words[static_cast<size_t>(bit >> 5)] >> (bit & 31)) & 1u) != 0u;
}

template <size_t N>
void put_field(std::array<uint32_t, N>& words, int lsb, int width, uint32_t value) {
  for (int bit = 0; bit < width; ++bit) put_bit(words, lsb + bit, ((value >> bit) & 1u) != 0u);
}

template <size_t N>
uint32_t get_field(const std::array<uint32_t, N>& words, int lsb, int width) {
  uint32_t value = 0;
  for (int bit = 0; bit < width; ++bit) {
    if (get_bit(words, lsb + bit)) value |= uint32_t{1} << bit;
  }
  return value;
}

template <size_t N, typename Wide>
std::array<uint32_t, N> copy_wide(const Wide& wide) {
  std::array<uint32_t, N> result{};
  for (size_t word = 0; word < N; ++word) result[word] = wide[word];
  return result;
}

template <size_t N, typename Wide>
void drive_wide(Wide& wide, const std::array<uint32_t, N>& value) {
  for (size_t word = 0; word < N; ++word) wide[word] = value[word];
}

bool joined_equal(const Joined& a, const Joined& b) {
  for (size_t word = 0; word < a.size(); ++word) {
    const uint32_t mask = (word == a.size() - 1) ? 0x00001FFFu : 0xFFFFFFFFu;
    if ((a[word] & mask) != (b[word] & mask)) return false;
  }
  return true;
}

struct Record {
  uint16_t owner;
  Logical logical;
  int32_t u;
  int32_t v;
  uint8_t page_generation;
};

Record make_record(uint32_t ordinal, uint8_t page_generation) {
  Record r{};
  r.owner = static_cast<uint16_t>(((ordinal * 173u) ^ 0x12A5u) & 0x3FFFu);
  for (size_t word = 0; word < r.logical.size(); ++word) {
    r.logical[word] = 0x9E3779B9u * static_cast<uint32_t>(word + 1) ^
                      (ordinal * 0x45D9F3Bu) ^ (0xA5A50000u + static_cast<uint32_t>(word) * 0x1111u);
  }
  // Only 31 bits exist in the ninth word.  Bits 23..30 are the sole page
  // generation location (logical bits 279..286).
  r.logical[8] &= 0x7FFFFFFFu;
  put_field(r.logical, kLogicalPageGenerationLsb, 8, page_generation);
  r.u = static_cast<int32_t>(0x11000000u + ordinal * 0x00010203u);
  r.v = static_cast<int32_t>(0xE2000000u - ordinal * 0x00030405u);
  r.page_generation = page_generation;
  return r;
}

Joined expected_joined(const Record& r) {
  Joined result{};
  put_field(result, 0, 32, static_cast<uint32_t>(r.v));
  put_field(result, 32, 32, static_cast<uint32_t>(r.u));
  for (int bit = 0; bit < kLogicalBits; ++bit)
    put_bit(result, 64 + bit, get_bit(r.logical, bit));
  put_field(result, 351, 14, r.owner);
  return result;
}

void drive_desc(Vtb_uv_join_v2_pair* d, const Record& r) {
  d->desc_owner_i = r.owner;
  drive_wide(d->desc_logical_i, r.logical);
}

void drive_uv(Vtb_uv_join_v2_pair* d, const Record& r) {
  d->uv_owner_i = r.owner;
  d->uv_u_i = static_cast<uint32_t>(r.u);
  d->uv_v_i = static_cast<uint32_t>(r.v);
}

void reset(Vtb_uv_join_v2_pair* d) {
  d->desc_valid_i = 0;
  d->uv_valid_i = 0;
  d->out_ready_i = 0;
  d->active_page_generation_i = 0;
  d->reload_desc_valid_i = 0;
  d->reload_uv_valid_i = 0;
  d->reload_out_ready_i = 0;
  d->rst_n = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);
  d->eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_uv_join_v2_pair* d = new Vtb_uv_join_v2_pair;
  d->clk = 0;
  reset(d);

  int layout_errors = 0;
  int production_hold_errors = 0;
  int mutant_control_errors = 0;
  int idle_errors = 0;

  zhao::check(d->good_idle_o == 1, "reset leaves all three production holds empty", 1,
              d->good_idle_o);
  zhao::check(d->good_mismatch_o == 0 && d->good_lifetime_fault_o == 0,
              "reset clears the owner-mismatch counter and its reset-lifetime structural fault", 0,
              static_cast<uint64_t>(d->good_mismatch_o) + d->good_lifetime_fault_o);

  // ==========================================================================
  // A/B STALL: A is held at output while B occupies both input holds.
  // ==========================================================================
  const Record a = make_record(1, 0x31);
  const Record b = make_record(2, 0xA6);
  const Joined a_expected = expected_joined(a);
  const Joined b_expected = expected_joined(b);

  // Descriptor A enters alone: the descriptor hold itself must make idle false.
  drive_desc(d, a);
  d->desc_valid_i = 1;
  d->uv_valid_i = 0;
  d->eval();
  const bool desc_a_fire = d->desc_valid_i && d->desc_ready_o;
  tick(d);
  d->desc_valid_i = 0;
  d->eval();
  if (!desc_a_fire || d->good_idle_o) ++idle_errors;

  // UV A enters later.  Descriptor B is already offered, but cannot overwrite
  // occupied descriptor A until A's pair transfers.
  drive_desc(d, b);
  drive_uv(d, a);
  d->desc_valid_i = 1;
  d->uv_valid_i = 1;
  d->active_page_generation_i = b.page_generation;
  d->eval();
  if (d->desc_ready_o != 0 || d->uv_ready_o != 1) ++production_hold_errors;
  tick(d);  // accepts UV A only

  // The complete A pair now transfers into the output while B replaces both
  // consumed input holds on the same edge.
  drive_uv(d, b);
  d->eval();
  if (!d->desc_ready_o || !d->uv_ready_o) ++production_hold_errors;
  tick(d);
  d->desc_valid_i = 0;  // B was accepted into both holds
  d->uv_valid_i = 0;
  d->eval();

  if (!d->good_valid_o || !d->generation_mutant_valid_o || !d->late_mutant_valid_o)
    ++production_hold_errors;

  const Joined good_a = copy_wide<12>(d->good_data_o);
  const Joined gen_a = copy_wide<12>(d->generation_mutant_data_o);
  const Joined late_a = copy_wide<12>(d->late_mutant_data_o);
  if (!joined_equal(good_a, a_expected)) ++layout_errors;

  Joined a_with_b_generation = a_expected;
  put_field(a_with_b_generation, kJoinedPageGenerationLsb, 8, b.page_generation);
  if (!joined_equal(gen_a, a_with_b_generation)) ++mutant_control_errors;
  if (!joined_equal(late_a, a_with_b_generation)) ++mutant_control_errors;

  // Every non-generation bit remains A in both mutants.  This proves the
  // controls isolate carriage rather than merely scrambling the packet.
  Joined gen_repaired = gen_a;
  Joined late_repaired = late_a;
  put_field(gen_repaired, kJoinedPageGenerationLsb, 8, a.page_generation);
  put_field(late_repaired, kJoinedPageGenerationLsb, 8, a.page_generation);
  if (!joined_equal(gen_repaired, a_expected)) ++mutant_control_errors;
  if (!joined_equal(late_repaired, a_expected)) ++mutant_control_errors;

  // Stall A for several clocks.  B remains in both input holds.  Production A
  // must be bit-stable.  A legal active-page change must visibly break ONLY the
  // late-global mutant, proving that detector's positive control can fire.
  const Joined good_stall_reference = good_a;
  const Joined gen_stall_reference = gen_a;
  const Joined late_stall_reference = late_a;
  for (int cycle = 0; cycle < 5; ++cycle) {
    if (cycle == 2) d->active_page_generation_i = 0x5C;
    d->eval();
    const Joined good_now = copy_wide<12>(d->good_data_o);
    const Joined gen_now = copy_wide<12>(d->generation_mutant_data_o);
    const Joined late_now = copy_wide<12>(d->late_mutant_data_o);
    if (!joined_equal(good_now, good_stall_reference)) ++production_hold_errors;
    if (!joined_equal(gen_now, gen_stall_reference)) ++mutant_control_errors;
    if (cycle < 2) {
      if (!joined_equal(late_now, late_stall_reference)) ++mutant_control_errors;
    } else {
      Joined want = a_expected;
      put_field(want, kJoinedPageGenerationLsb, 8, 0x5C);
      if (!joined_equal(late_now, want)) ++mutant_control_errors;
    }
    if (d->good_idle_o) ++idle_errors;
    tick(d);
  }

  // Accept A.  The already-held B pair reloads the output on that SAME edge,
  // so valid remains asserted and B follows without a bubble.
  d->active_page_generation_i = b.page_generation;
  d->out_ready_i = 1;
  d->eval();
  if (!d->good_valid_o || !joined_equal(copy_wide<12>(d->good_data_o), a_expected))
    ++production_hold_errors;
  tick(d);
  d->eval();
  if (!d->good_valid_o || !joined_equal(copy_wide<12>(d->good_data_o), b_expected))
    ++production_hold_errors;
  tick(d);  // accept B
  d->out_ready_i = 0;
  d->eval();
  if (!d->good_idle_o) ++idle_errors;

  std::printf("  A/B hold: layout errors %d, production hold errors %d, mutant control errors %d\n",
              layout_errors, production_hold_errors, mutant_control_errors);

  zhao::check(layout_errors == 0,
              "joined365 is exactly {owner14,logical287,U32,V32}, including the sole "
              "generation at logical287[286:279]",
              0, layout_errors);
  zhao::check(production_hold_errors == 0,
              "production A holds all 365 bits while B is accepted behind it, and B "
              "reloads on A's retirement edge without a bubble",
              0, production_hold_errors);
  zhao::check(mutant_control_errors == 0,
              "both committed controls fire in exactly one direction: B-generation/A-owner "
              "and late-active-generation/A-owner, with every other A bit intact",
              0, mutant_control_errors);
  zhao::check(idle_errors == 0,
              "idle is low for a descriptor hold and for held input/output work, then true "
              "only after all three registers empty",
              0, idle_errors);
  const bool legal_ab_identity_clean =
      d->good_mismatch_o == 0 && !d->good_lifetime_fault_o &&
      d->generation_mutant_mismatch_o == 0 && !d->generation_mutant_lifetime_fault_o &&
      d->late_mutant_mismatch_o == 0 && !d->late_mutant_lifetime_fault_o;
  zhao::check(legal_ab_identity_clean,
              "the legal A/B output-hold schedule produces zero owner mismatch and zero "
              "lifetime fault before reset; mutant payload differences are not identity faults",
              1, legal_ab_identity_clean ? 1 : 0);

  // ==========================================================================
  // FULL-IDENTITY POSITIVE CONTROL: captured A/B beats matching live C/C.
  // ==========================================================================
  auto captured_mismatch_control = [&](bool stall_output, uint32_t ordinal) {
    reset(d);
    bool ok = true;
    const Record desc_a = make_record(ordinal, static_cast<uint8_t>(0x31u + ordinal));
    Record uv_b = make_record(ordinal + 1, static_cast<uint8_t>(0x91u + ordinal));
    if (uv_b.owner == desc_a.owner) uv_b.owner ^= 0x0001u;
    const Record live_c = make_record(ordinal + 2, static_cast<uint8_t>(0xD1u + ordinal));
    const Record prior = make_record(ordinal + 3, static_cast<uint8_t>(0x61u + ordinal));

    d->out_ready_i = stall_output ? 0 : 1;
    Joined held_prior{};

    // The stalled variant first fills the output with a genuinely valid prior
    // record.  Merely driving ready low against an empty output would leave
    // out_room true and would not test mismatch disposal behind a full register.
    if (stall_output) {
      drive_desc(d, prior);
      drive_uv(d, prior);
      d->active_page_generation_i = prior.page_generation;
      d->desc_valid_i = 1;
      d->uv_valid_i = 1;
      d->eval();
      ok &= d->desc_ready_o && d->uv_ready_o;
      tick(d);  // accept prior halves
      d->desc_valid_i = 0;
      d->uv_valid_i = 0;
      tick(d);  // move prior into the output register
      d->eval();
      held_prior = copy_wide<12>(d->good_data_o);
      ok &= d->good_valid_o && joined_equal(held_prior, expected_joined(prior));
    }

    // Descriptor A is accepted by itself and then held.  UV B does not appear
    // until a later cycle, so the two detector operands have distinct capture
    // enables rather than moving together.  In the stalled variant the prior
    // output remains valid throughout both captures.
    drive_desc(d, desc_a);
    d->desc_valid_i = 1;
    d->uv_valid_i = 0;
    d->eval();
    ok &= d->desc_ready_o && d->good_mismatch_o == 0;
    if (stall_output)
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
    tick(d);
    d->desc_valid_i = 0;
    d->eval();
    ok &= !d->desc_ready_o && d->uv_ready_o && !d->good_idle_o;
    if (stall_output)
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);

    drive_uv(d, uv_b);
    d->uv_valid_i = 1;
    d->eval();
    ok &= d->uv_ready_o && d->good_mismatch_o == 0;
    if (stall_output)
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
    tick(d);
    d->uv_valid_i = 0;

    // BEFORE the captured mismatch is detected, replace every live pin with a
    // matching C/C offer. A detector comparing live inputs would now see no
    // fault; the correct detector compares held descriptor A with held UV B.
    // The held mismatch must NOT acknowledge these replacements.
    drive_desc(d, live_c);
    drive_uv(d, live_c);
    d->desc_valid_i = 1;
    d->uv_valid_i = 1;
    d->active_page_generation_i = live_c.page_generation;
    d->eval();
    ok &= !d->desc_ready_o && !d->uv_ready_o;
    if (stall_output)
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
    tick(d);  // detects held A/B, discards it, and raises the reset barrier
    d->eval();
    ok &= d->good_mismatch_o == 1 && d->good_lifetime_fault_o;
    ok &= d->generation_mutant_mismatch_o == 1 && d->generation_mutant_lifetime_fault_o;
    ok &= d->late_mutant_mismatch_o == 1 && d->late_mutant_lifetime_fault_o;
    ok &= !d->desc_ready_o && !d->uv_ready_o;

    if (stall_output) {
      // The unrelated prior output remains bit-stable, but C/C is never accepted
      // behind the corrupted pair and both input ready outputs stay closed.
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
      for (int cycle = 0; cycle < 4; ++cycle) {
        d->eval();
        ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
        ok &= d->good_mismatch_o == 1 && !d->desc_ready_o && !d->uv_ready_o;
        tick(d);
      }

      // The already-valid prior record may retire; the blocked C/C offer may not
      // reload the output on that edge or any later edge.
      d->out_ready_i = 1;
      d->eval();
      ok &= d->good_valid_o && joined_equal(copy_wide<12>(d->good_data_o), held_prior);
      tick(d);
      d->eval();
      ok &= !d->good_valid_o && !d->desc_ready_o && !d->uv_ready_o;
    } else {
      // With no prior output, the mismatch produces no output and C/C remains an
      // unaccepted external offer behind the lifetime barrier.
      for (int cycle = 0; cycle < 4; ++cycle) {
        d->eval();
        ok &= !d->good_valid_o && !d->desc_ready_o && !d->uv_ready_o;
        ok &= d->good_mismatch_o == 1;
        tick(d);
      }
    }

    d->eval();
    ok &= d->good_mismatch_o == 1 && d->generation_mutant_mismatch_o == 1 &&
          d->late_mutant_mismatch_o == 1;
    ok &= d->good_idle_o && d->generation_mutant_idle_o && d->late_mutant_idle_o;
    return ok;
  };

  const bool mismatch_ready = captured_mismatch_control(false, 9);
  zhao::check(mismatch_ready,
              "descriptor A and later UV B mismatch exactly once after live pins become "
              "matching C/C; no same-edge replacement is accepted and no output escapes",
              1, mismatch_ready ? 1 : 0);

  const bool mismatch_stalled = captured_mismatch_control(true, 19);
  zhao::check(mismatch_stalled,
              "with a genuine prior output held, captured A/B is discarded and counted once "
              "without accepting C/C; the prior may retire but no post-fault output reloads",
              1, mismatch_stalled ? 1 : 0);

  // The second control leaves the matching C/C offer asserted and stable. Prove
  // that ordinary activity cannot cross or clear the reset-lifetime barrier.
  const uint32_t sticky_good_count = d->good_mismatch_o;
  const uint32_t sticky_generation_count = d->generation_mutant_mismatch_o;
  const uint32_t sticky_late_count = d->late_mutant_mismatch_o;
  bool ordinary_is_blocked = true;
  for (int cycle = 0; cycle < 6; ++cycle) {
    d->out_ready_i = 1;
    d->eval();
    ordinary_is_blocked &= !d->desc_ready_o && !d->uv_ready_o && !d->good_valid_o;
    ordinary_is_blocked &= d->good_lifetime_fault_o &&
        d->generation_mutant_lifetime_fault_o && d->late_mutant_lifetime_fault_o;
    ordinary_is_blocked &= d->good_mismatch_o == sticky_good_count &&
        d->generation_mutant_mismatch_o == sticky_generation_count &&
        d->late_mutant_mismatch_o == sticky_late_count;
    ordinary_is_blocked &= d->good_idle_o;
    tick(d);
  }
  zhao::check(ordinary_is_blocked,
              "stable legal traffic after owner mismatch receives no ready and emits no output; "
              "only reset can reopen the join or clear lifetime state",
              1, ordinary_is_blocked ? 1 : 0);

  reset(d);
  const bool reset_clears_lifetime =
      d->good_mismatch_o == 0 && !d->good_lifetime_fault_o && d->good_idle_o &&
      d->generation_mutant_mismatch_o == 0 &&
      !d->generation_mutant_lifetime_fault_o && d->generation_mutant_idle_o &&
      d->late_mutant_mismatch_o == 0 && !d->late_mutant_lifetime_fault_o &&
      d->late_mutant_idle_o;
  zhao::check(reset_clears_lifetime,
              "rst_n is the only event that clears UV mismatch lifetime state", 1,
              reset_clears_lifetime ? 1 : 0);

  // ==========================================================================
  // INDEPENDENT STREAMS + RANDOM OUTPUT STALLS.
  // ==========================================================================
  constexpr int kRecords = 192;
  std::vector<Record> records;
  records.reserve(kRecords);
  for (int i = 0; i < kRecords; ++i)
    records.push_back(make_record(static_cast<uint32_t>(i + 20), static_cast<uint8_t>(1 + i)));

  int desc_sent = 0;
  int uv_sent = 0;
  int outputs = 0;
  int stream_errors = 0;
  int stalled_checks = 0;
  int desc_source_hold_checks = 0;
  int uv_source_hold_checks = 0;
  uint32_t rng = 0xC001D00Du;
  bool previous_stall = false;
  Joined previous_stalled_data{};

  // The two producers start offers independently.  Once either asserts valid,
  // it retains valid and the complete payload until its own handshake.
  bool desc_offer = false;
  bool uv_offer = false;
  int desc_start_gap = 0;
  int uv_start_gap = 12;  // guaranteed long descriptor-leading record 0
  bool desc_was_stalled = false;
  bool uv_was_stalled = false;
  uint16_t stalled_desc_owner = 0;
  Logical stalled_desc_logical{};
  uint16_t stalled_uv_owner = 0;
  uint32_t stalled_uv_u = 0;
  uint32_t stalled_uv_v = 0;
  std::vector<int> desc_accept_cycle(kRecords, -1);
  std::vector<int> uv_accept_cycle(kRecords, -1);

  for (int cycle = 0; cycle < 20000 && outputs < kRecords; ++cycle) {
    rng = rng * 1664525u + 1013904223u;

    if (!desc_offer && desc_sent < kRecords) {
      if (desc_start_gap == 0)
        desc_offer = true;
      else
        --desc_start_gap;
    }
    if (!uv_offer && uv_sent < kRecords) {
      if (uv_start_gap == 0)
        uv_offer = true;
      else
        --uv_start_gap;
    }

    d->desc_valid_i = desc_offer;
    d->uv_valid_i = uv_offer;
    if (desc_offer) drive_desc(d, records[static_cast<size_t>(desc_sent)]);
    if (uv_offer) drive_uv(d, records[static_cast<size_t>(uv_sent)]);

    // Check the producer-side hold BEFORE consulting current ready.  This catches
    // a driver which silently advances payload while yesterday's valid remained
    // stalled, independently of eventual output correctness.
    if (desc_was_stalled) {
      ++desc_source_hold_checks;
      if (!d->desc_valid_i || d->desc_owner_i != stalled_desc_owner ||
          copy_wide<9>(d->desc_logical_i) != stalled_desc_logical)
        ++stream_errors;
    }
    if (uv_was_stalled) {
      ++uv_source_hold_checks;
      if (!d->uv_valid_i || d->uv_owner_i != stalled_uv_owner ||
          d->uv_u_i != stalled_uv_u || d->uv_v_i != stalled_uv_v)
        ++stream_errors;
    }

    d->out_ready_i = ((rng >> 9) & 3u) != 0u;
    d->active_page_generation_i = static_cast<uint8_t>(rng >> 24);
    d->eval();

    const bool desc_fire = d->desc_valid_i && d->desc_ready_o;
    const bool uv_fire = d->uv_valid_i && d->uv_ready_o;

    desc_was_stalled = d->desc_valid_i && !d->desc_ready_o;
    if (desc_was_stalled) {
      stalled_desc_owner = d->desc_owner_i;
      stalled_desc_logical = copy_wide<9>(d->desc_logical_i);
    }
    uv_was_stalled = d->uv_valid_i && !d->uv_ready_o;
    if (uv_was_stalled) {
      stalled_uv_owner = d->uv_owner_i;
      stalled_uv_u = d->uv_u_i;
      stalled_uv_v = d->uv_v_i;
    }

    if (d->good_valid_o) {
      const Joined got = copy_wide<12>(d->good_data_o);
      if (outputs >= kRecords ||
          !joined_equal(got, expected_joined(records[static_cast<size_t>(outputs)])))
        ++stream_errors;
      if (previous_stall && !joined_equal(got, previous_stalled_data)) ++stream_errors;
      if (!d->out_ready_i) {
        previous_stalled_data = got;
        previous_stall = true;
        ++stalled_checks;
      } else {
        previous_stall = false;
        ++outputs;
      }
    } else {
      if (previous_stall) ++stream_errors;
      previous_stall = false;
    }

    tick(d);
    if (desc_fire) {
      desc_accept_cycle[static_cast<size_t>(desc_sent)] = cycle;
      ++desc_sent;
      desc_offer = false;
      // Deterministic early cases force both source-hold polarities; later
      // starts use independent pseudo-random gaps.
      if (desc_sent == 1)
        desc_start_gap = 32;  // record 1: long UV lead
      else if ((desc_sent == 2) || (desc_sent == 3))
        desc_start_gap = 0;   // record 3 waits behind descriptor-held record 2
      else if (desc_sent == 4)
        desc_start_gap = 20;  // record 4: long UV lead; UV 5 waits behind it
      else
        desc_start_gap = static_cast<int>((rng >> 3) & 7u);
    }
    if (uv_fire) {
      uv_accept_cycle[static_cast<size_t>(uv_sent)] = cycle;
      ++uv_sent;
      uv_offer = false;
      if (uv_sent == 1)
        uv_start_gap = 0;     // record 1 enters before delayed descriptor 1
      else if (uv_sent == 2)
        uv_start_gap = 40;    // record 2 waits while descriptor 3 is offered
      else if ((uv_sent == 3) || (uv_sent == 4) || (uv_sent == 5))
        uv_start_gap = 0;     // record 5 waits behind UV-held record 4
      else
        uv_start_gap = static_cast<int>((rng >> 19) & 7u);
    }
  }
  d->desc_valid_i = 0;
  d->uv_valid_i = 0;
  d->out_ready_i = 0;
  d->eval();

  const int descriptor_lead = uv_accept_cycle[0] - desc_accept_cycle[0];
  const int uv_lead = desc_accept_cycle[1] - uv_accept_cycle[1];
  std::printf(
      "  randomized starts: desc %d, uv %d, outputs %d | output stalls %d, "
      "source holds d/u %d/%d | forced leads d/u %d/%d | errors %d\n",
      desc_sent, uv_sent, outputs, stalled_checks, desc_source_hold_checks,
      uv_source_hold_checks, descriptor_lead, uv_lead, stream_errors);
  zhao::check(desc_sent == kRecords && uv_sent == kRecords && outputs == kRecords,
              "both independently started input streams and every output record completed",
              kRecords * 3, static_cast<uint64_t>(desc_sent + uv_sent + outputs));
  zhao::check(descriptor_lead >= 10 && uv_lead >= 10,
              "the schedule contains guaranteed long descriptor-leading and UV-leading cases",
              1, (descriptor_lead >= 10 && uv_lead >= 10) ? 1 : 0);
  zhao::check(desc_source_hold_checks > 0 && uv_source_hold_checks > 0,
              "both producers were backpressured and held valid plus their full payload to "
              "their own handshake",
              1, (desc_source_hold_checks > 0 && uv_source_hold_checks > 0) ? 1 : 0);
  zhao::check(stalled_checks > 50,
              "the output payload was checked on many stalled cycles, not only acceptance",
              1, stalled_checks > 50 ? 1 : 0);
  zhao::check(stream_errors == 0,
              "independent randomized offers preserve producer holds and every joined owner, "
              "descriptor, U and V under output backpressure",
              0, stream_errors);
  zhao::check(d->good_mismatch_o == 0 && !d->good_lifetime_fault_o,
              "legal independently started streams produce no identity fault", 0,
              static_cast<uint64_t>(d->good_mismatch_o) + d->good_lifetime_fault_o);
  zhao::check(d->good_idle_o,
              "after randomized drain, descriptor hold, UV hold, and output hold are all empty",
              1, d->good_idle_o);

  // ==========================================================================
  // UNINTERRUPTED PREPARED OUTPUT: one accepted result each clock.
  // ==========================================================================
  reset(d);
  constexpr int kBurst = 64;
  std::vector<Record> burst;
  for (int i = 0; i < kBurst; ++i)
    burst.push_back(make_record(static_cast<uint32_t>(i + 400), static_cast<uint8_t>(0x80 + i)));

  desc_sent = 0;
  uv_sent = 0;
  outputs = 0;
  int cadence_bubbles = 0;
  int previous_output_cycle = -1;
  for (int cycle = 0; cycle < 300 && outputs < kBurst; ++cycle) {
    d->desc_valid_i = desc_sent < kBurst;
    d->uv_valid_i = uv_sent < kBurst;
    if (desc_sent < kBurst) drive_desc(d, burst[static_cast<size_t>(desc_sent)]);
    if (uv_sent < kBurst) drive_uv(d, burst[static_cast<size_t>(uv_sent)]);
    d->out_ready_i = 1;
    d->eval();
    const bool desc_fire = d->desc_valid_i && d->desc_ready_o;
    const bool uv_fire = d->uv_valid_i && d->uv_ready_o;
    if (d->good_valid_o) {
      if (previous_output_cycle >= 0 && cycle != previous_output_cycle + 1) ++cadence_bubbles;
      previous_output_cycle = cycle;
      if (!joined_equal(copy_wide<12>(d->good_data_o),
                        expected_joined(burst[static_cast<size_t>(outputs)])))
        ++cadence_bubbles;
      ++outputs;
    }
    tick(d);
    if (desc_fire) ++desc_sent;
    if (uv_fire) ++uv_sent;
  }
  d->desc_valid_i = 0;
  d->uv_valid_i = 0;
  d->out_ready_i = 0;
  d->eval();

  std::printf("  all-ready cadence: %d outputs, bubbles %d\n", outputs, cadence_bubbles);
  zhao::check(outputs == kBurst, "the all-ready burst emitted every joined record", kBurst, outputs);
  zhao::check(cadence_bubbles == 0,
              "after pipeline fill, the registered join emits one result every clock with "
              "same-edge pop/reload",
              0, cadence_bubbles);
  zhao::check(d->good_idle_o, "the cadence burst drains back to exact idle", 1, d->good_idle_o);

  // Inverse-polarity fire control: the renamed mutant forbids a retiring output
  // from reloading on the same edge.  Payload/order still match, but every
  // adjacent result must be separated by an observable empty cycle.
  reset(d);
  int reload_sent = 0;
  int reload_outputs = 0;
  int reload_bubbles = 0;
  int reload_payload_errors = 0;
  int reload_previous_output_cycle = -1;
  for (int cycle = 0; cycle < 500 && reload_outputs < kBurst; ++cycle) {
    d->desc_valid_i = 0;
    d->uv_valid_i = 0;
    d->out_ready_i = 0;
    d->reload_desc_valid_i = reload_sent < kBurst;
    d->reload_uv_valid_i = reload_sent < kBurst;
    d->reload_out_ready_i = 1;
    if (reload_sent < kBurst) {
      drive_desc(d, burst[static_cast<size_t>(reload_sent)]);
      drive_uv(d, burst[static_cast<size_t>(reload_sent)]);
    }
    d->eval();
    const bool reload_desc_fire = d->reload_desc_valid_i && d->reload_desc_ready_o;
    const bool reload_uv_fire = d->reload_uv_valid_i && d->reload_uv_ready_o;
    if (reload_desc_fire != reload_uv_fire) ++reload_payload_errors;

    if (d->reload_mutant_valid_o) {
      if (reload_previous_output_cycle >= 0 &&
          cycle != reload_previous_output_cycle + 1)
        ++reload_bubbles;
      reload_previous_output_cycle = cycle;
      if (!joined_equal(copy_wide<12>(d->reload_mutant_data_o),
                        expected_joined(burst[static_cast<size_t>(reload_outputs)])))
        ++reload_payload_errors;
      ++reload_outputs;
    }

    tick(d);
    if (reload_desc_fire && reload_uv_fire) ++reload_sent;
  }
  d->reload_desc_valid_i = 0;
  d->reload_uv_valid_i = 0;
  d->reload_out_ready_i = 0;
  d->eval();

  std::printf("  no-reload mutant cadence: %d outputs, observed bubbles %d, payload errors %d\n",
              reload_outputs, reload_bubbles, reload_payload_errors);
  zhao::check(reload_sent == kBurst && reload_outputs == kBurst,
              "the no-reload mutant still accepts and emits the entire cadence burst",
              kBurst * 2, static_cast<uint64_t>(reload_sent + reload_outputs));
  zhao::check(reload_payload_errors == 0 && d->reload_mutant_mismatch_o == 0 &&
                  !d->reload_mutant_lifetime_fault_o,
              "the no-reload mutation changes cadence only, not payload/order/identity",
              0, static_cast<uint64_t>(reload_payload_errors) + d->reload_mutant_mismatch_o +
                     d->reload_mutant_lifetime_fault_o);
  zhao::check(reload_bubbles == kBurst - 1,
              "inverse-polarity control observes one forbidden bubble between every adjacent "
              "result when same-edge reload is removed",
              kBurst - 1, reload_bubbles);
  zhao::check(d->reload_mutant_idle_o,
              "the no-reload mutant drains back to exact idle after exposing its bubbles",
              1, d->reload_mutant_idle_o);

  const int rc = zhao::report_and_exit("texture_uv_join_v2_directed");
  delete d;
  zhao::exit_hard(rc);
}
