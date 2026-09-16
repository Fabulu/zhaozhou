// texture_early_desc_v2_directed.cpp -- exact 287->320 descriptor contract.
#if defined(ZHAO_DESC_PAD_MUTANT)
#include "Vzhao_texture_early_desc_v2_pad_mutant.h"
using DescriptorDut = Vzhao_texture_early_desc_v2_pad_mutant;
#elif defined(ZHAO_DESC_SLOTSWAP_MUTANT)
#include "Vzhao_texture_early_desc_v2_slotswap_mutant.h"
using DescriptorDut = Vzhao_texture_early_desc_v2_slotswap_mutant;
#else
#include "Vzhao_texture_early_desc_v2.h"
using DescriptorDut = Vzhao_texture_early_desc_v2;
#endif

#include <array>
#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

namespace {

void tick(DescriptorDut* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

struct Row {
  std::array<uint32_t, 7> aux{};
  uint8_t lod, cls, aux_required, count, palette_slot, palette_generation;
  uint8_t material_a, material_b, mosaic_weight, selector, page_generation;
  uint8_t owner_generation;
};

Row make_row(unsigned seed) {
  Row r{};
  for (unsigned i = 0; i < r.aux.size(); ++i)
    r.aux[i] = 0x10203040u ^ (seed * 0x01010101u) ^ (i * 0x11182731u);
  r.lod = static_cast<uint8_t>(0x10u + seed);
  r.cls = static_cast<uint8_t>(seed & 3u);
  r.aux_required = static_cast<uint8_t>((seed >> 1) & 1u);
  r.count = static_cast<uint8_t>((seed + 1u) & 3u);
  r.palette_slot = static_cast<uint8_t>((seed + 2u) & 3u);
  r.palette_generation = static_cast<uint8_t>(0x40u + seed);
  r.material_a = static_cast<uint8_t>(0x50u + seed);
  r.material_b = static_cast<uint8_t>(0x70u + seed);
  r.mosaic_weight = static_cast<uint8_t>(0x90u + seed);
  r.selector = static_cast<uint8_t>(0xB0u + seed);
  r.page_generation = static_cast<uint8_t>(0xD0u + seed);
  r.owner_generation = static_cast<uint8_t>(0x20u + seed);
  return r;
}

void put_bits(std::array<uint32_t, 9>& words, unsigned lo, unsigned width, uint32_t value) {
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned dst = lo + bit;
    if ((value >> bit) & 1u) words[dst / 32] |= 1u << (dst % 32);
  }
}

std::array<uint32_t, 9> logical_row(const Row& r) {
  std::array<uint32_t, 9> words{};
  for (unsigned i = 0; i < 7; ++i) words[i] = r.aux[i];
  put_bits(words, 224, 8, r.lod);
  put_bits(words, 232, 2, r.cls);
  put_bits(words, 234, 1, r.aux_required);
  put_bits(words, 235, 2, r.count);
  put_bits(words, 237, 2, r.palette_slot);
  put_bits(words, 239, 8, r.palette_generation);
  put_bits(words, 247, 8, r.material_a);
  put_bits(words, 255, 8, r.material_b);
  put_bits(words, 263, 8, r.mosaic_weight);
  put_bits(words, 271, 8, r.selector);
  put_bits(words, 279, 8, r.page_generation);
  words[8] &= 0x7FFFFFFFu;  // logical bit 287 does not exist
  return words;
}

uint16_t owner(unsigned slot, uint8_t generation) {
  return static_cast<uint16_t>((slot << 8) | generation);
}

void drive_write(DescriptorDut* d, unsigned slot, const Row& r) {
  d->wr_valid_i = 1;
  d->wr_slot_i = slot;
  d->wr_owner_generation_i = r.owner_generation;
  for (unsigned i = 0; i < 7; ++i) d->wr_aux_context_i[i] = r.aux[i];
  d->wr_lod_q4_4_i = r.lod;
  d->wr_response_class_i = r.cls;
  d->wr_aux_required_i = r.aux_required;
  d->wr_sample_count_i = r.count;
  d->wr_palette_slot_i = r.palette_slot;
  d->wr_palette_generation_i = r.palette_generation;
  d->wr_mosaic_material_a_i = r.material_a;
  d->wr_mosaic_material_b_i = r.material_b;
  d->wr_mosaic_weight_i = r.mosaic_weight;
  d->wr_binding_selector_i = r.selector;
  d->wr_active_page_generation_i = r.page_generation;
  tick(d);
  d->wr_valid_i = 0;
  d->eval();
}

void launch_read(DescriptorDut* d, unsigned slot, uint8_t generation) {
  d->rd_valid_i = 1;
  d->rd_owner_i = owner(slot, generation);
  d->eval();
  zhao::check(d->rd_ready_o != 0, "descriptor read storage was available", 1, d->rd_ready_o);
  tick(d);
  d->rd_valid_i = 0;
  d->eval();
}

unsigned logical_mismatches(DescriptorDut* d, const std::array<uint32_t, 9>& expected) {
  unsigned errors = 0;
  for (unsigned i = 0; i < 9; ++i) {
    uint32_t mask = (i == 8) ? 0x7FFFFFFFu : 0xFFFFFFFFu;
    if ((d->rd_logical_o[i] & mask) != (expected[i] & mask)) ++errors;
  }
  return errors;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* d = new DescriptorDut;
  d->clk = 0;
  d->rst_n = 0;
  d->frame_fault_clear_i = 0;
  d->wr_valid_i = 0;
  d->rd_valid_i = 0;
  d->rd_result_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  zhao::check(d->desc_pad_fault_o == 0, "the reset-only 32-bit pad counter starts at zero", 0,
              d->desc_pad_fault_o);
  zhao::check(d->rd_generation_mismatch_o == 0,
              "the generation counter independently starts at zero", 0,
              d->rd_generation_mismatch_o);
  zhao::check(d->idle_o != 0, "the descriptor is idle after reset", 1, d->idle_o);

  const Row a = make_row(3);
  const Row b = make_row(41);
  drive_write(d, 7, a);
  drive_write(d, 29, b);

  launch_read(d, 7, a.owner_generation);
  zhao::check(d->rd_result_valid_o != 0, "an accepted synchronous read produces one held response",
              1, d->rd_result_valid_o);
  tick(d);  // one-shot verdict while held

#if defined(ZHAO_DESC_PAD_MUTANT)
  zhao::check(d->rd_descriptor_pad_ok_o == 0,
              "PAD MUTANT FIRE: its one nonzero physical pad bit is detected", 0,
              d->rd_descriptor_pad_ok_o);
  zhao::check(d->rd_descriptor_usable_o == 0,
              "PAD MUTANT FIRE: no corrupt logical bit is exposed as usable", 0,
              d->rd_descriptor_usable_o);
  zhao::check(d->desc_pad_fault_o == 1,
              "PAD MUTANT FIRE: one accepted bad-pad read increments exactly once", 1,
              d->desc_pad_fault_o);
  tick(d);
  tick(d);
  zhao::check(d->desc_pad_fault_o == 1,
              "PAD MUTANT FIRE: output stalls cannot recount the same read", 1,
              d->desc_pad_fault_o);

  d->rd_result_ready_i = 1;
  tick(d);
  d->rd_result_ready_i = 0;
  launch_read(d, 29, b.owner_generation);
  tick(d);
  zhao::check(d->desc_pad_fault_o == 2,
              "PAD MUTANT FIRE: two distinct accepted bad-pad reads produce delta two", 2,
              d->desc_pad_fault_o);
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->frame_fault_o == 0 && d->desc_pad_fault_o == 2,
              "PAD MUTANT FIRE: sticky clear does not clear the reset-only counter", 1,
              (!d->frame_fault_o && d->desc_pad_fault_o == 2) ? 1 : 0);

  // Verilator exposes this output register directly for the bounded modulo-wrap
  // control; driving 2^32 physical reads would not be a useful acceptance test.
  d->desc_pad_fault_o = 0xFFFFFFFFu;
  d->eval();
  d->rd_result_ready_i = 1;
  tick(d);
  d->rd_result_ready_i = 0;
  launch_read(d, 7, a.owner_generation);
  tick(d);
  zhao::check(d->desc_pad_fault_o == 0,
              "PAD MUTANT FIRE: 32-bit counter wraps FFFFFFFF to zero modulo 2^32", 0,
              d->desc_pad_fault_o);
#else
  zhao::check(d->rd_descriptor_pad_ok_o != 0,
              "all 33 explicitly written physical pad bits read as zero", 1,
              d->rd_descriptor_pad_ok_o);
  zhao::check(d->rd_owner_generation_ok_o != 0, "the independently stored owner generation matches",
              1, d->rd_owner_generation_ok_o);
  zhao::check(d->rd_descriptor_usable_o != 0, "both independent verdicts make the row usable", 1,
              d->rd_descriptor_usable_o);
  zhao::check(logical_mismatches(d, logical_row(a)) == 0,
              "all 287 logical bits retain the frozen low-first layout", 0,
              logical_mismatches(d, logical_row(a)));

  // Hold A while a distinguishable B owner/address is physically offered.
  d->rd_valid_i = 1;
  d->rd_owner_i = owner(29, b.owner_generation);
  d->eval();
  zhao::check(d->rd_ready_o == 0, "the occupied response withholds another descriptor acceptance",
              0, d->rd_ready_o);
  tick(d);
  tick(d);
#if defined(ZHAO_DESC_SLOTSWAP_MUTANT)
  zhao::check(logical_mismatches(d, logical_row(a)) != 0,
              "SLOTSWAP MUTANT FIRE: offered B overwrote held A payload", 1,
              logical_mismatches(d, logical_row(a)) != 0);
#else
  zhao::check(d->rd_owner_o == owner(7, a.owner_generation),
              "the held response keeps A's independently captured owner",
              owner(7, a.owner_generation), d->rd_owner_o);
  zhao::check(logical_mismatches(d, logical_row(a)) == 0,
              "the held eight-slice payload cannot follow offered B", 0,
              logical_mismatches(d, logical_row(a)));
#endif
  d->rd_valid_i = 0;

  // Retire A, then prove a stale generation is terminally unusable and counted
  // once even while its response is stalled.
  d->rd_result_ready_i = 1;
  tick(d);
  d->rd_result_ready_i = 0;
  launch_read(d, 29, static_cast<uint8_t>(b.owner_generation ^ 0x5Au));
  tick(d);
  zhao::check(d->rd_owner_generation_ok_o == 0,
              "a stale independently offered generation fails its verdict", 0,
              d->rd_owner_generation_ok_o);
  zhao::check(d->rd_descriptor_usable_o == 0, "a stale row exposes no logical descriptor", 0,
              d->rd_descriptor_usable_o);
  std::array<uint32_t, 9> zero{};
  zhao::check(logical_mismatches(d, zero) == 0,
              "the stale row's entire 287-bit output is canonical zero", 0,
              logical_mismatches(d, zero));
  zhao::check(d->rd_generation_mismatch_o == 1,
              "one stale accepted read increments its own modulo counter once", 1,
              d->rd_generation_mismatch_o);
  tick(d);
  zhao::check(d->rd_generation_mismatch_o == 1, "a stalled stale response is not recounted", 1,
              d->rd_generation_mismatch_o);
  zhao::check(d->frame_fault_o != 0, "the same verdict independently sets sticky frame fault", 1,
              d->frame_fault_o);
  d->frame_fault_clear_i = 1;
  tick(d);
  d->frame_fault_clear_i = 0;
  d->eval();
  zhao::check(d->frame_fault_o == 0, "sticky frame state can clear independently", 0,
              d->frame_fault_o);
  zhao::check(d->rd_generation_mismatch_o == 1,
              "clearing sticky state does not clear a reset-only counter", 1,
              d->rd_generation_mismatch_o);
  zhao::check(d->desc_pad_fault_o == 0, "legal writes cannot create a pad event", 0,
              d->desc_pad_fault_o);
#endif

  zhao::check(d->writes_o == 2, "both descriptor writes use the supplied owner slots", 2,
              d->writes_o);

  const int rc = zhao::report_and_exit("texture_early_desc_v2_directed");
  delete d;
  zhao::exit_hard(rc);
}
