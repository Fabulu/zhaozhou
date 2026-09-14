// texture_frag_expand_v2_directed.cpp -- owner-mask expansion and carriage.
#if defined(ZHAO_FRAG_EXPAND_QUEUE_MUTANT)
#include "Vzhao_texture_frag_expand_v2_queue_guard_mutant.h"
using ExpandDut = Vzhao_texture_frag_expand_v2_queue_guard_mutant;
#elif defined(ZHAO_FRAG_EXPAND_SELECTOR_MUTANT)
#include "Vzhao_texture_frag_expand_v2_mutant.h"
using ExpandDut = Vzhao_texture_frag_expand_v2_mutant;
#else
#include "Vzhao_texture_frag_expand_v2.h"
using ExpandDut = Vzhao_texture_frag_expand_v2;
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

namespace {

using Words287 = std::array<uint32_t, 9>;
using Aux224 = std::array<uint32_t, 7>;

void tick(ExpandDut* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

void put_bits(Words287& words, unsigned lo, unsigned width, uint32_t value) {
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned dst = lo + bit;
    if ((value >> bit) & 1u) words[dst / 32] |= 1u << (dst % 32);
  }
}

struct Descriptor {
  Aux224 aux{};
  uint8_t lod = 0, cls = 0, aux_required = 0, count = 0;
  uint8_t palette_slot = 0, palette_generation = 0;
  uint8_t material_a = 0, material_b = 0, mosaic_weight = 0;
  uint8_t selector = 0, page_generation = 0;
};

Words287 pack(const Descriptor& d) {
  Words287 words{};
  for (unsigned i = 0; i < 7; ++i) words[i] = d.aux[i];
  put_bits(words, 224, 8, d.lod);
  put_bits(words, 232, 2, d.cls);
  put_bits(words, 234, 1, d.aux_required);
  put_bits(words, 235, 2, d.count);
  put_bits(words, 237, 2, d.palette_slot);
  put_bits(words, 239, 8, d.palette_generation);
  put_bits(words, 247, 8, d.material_a);
  put_bits(words, 255, 8, d.material_b);
  put_bits(words, 263, 8, d.mosaic_weight);
  put_bits(words, 271, 8, d.selector);
  put_bits(words, 279, 8, d.page_generation);
  words[8] &= 0x7FFFFFFFu;
  return words;
}

uint8_t descriptor_mask(const Descriptor& d) {
  const uint8_t samples = d.count == 0 ? 0 : d.count == 1 ? 1 : d.count == 2 ? 3 : 7;
  return static_cast<uint8_t>((d.aux_required ? 8 : 0) | samples);
}

bool aux_zero(const Aux224& a) {
  for (uint32_t word : a)
    if (word != 0) return false;
  return true;
}

struct Work {
  uint16_t owner;
  int32_t u, v;
  Descriptor desc;
  uint8_t required_mask;
  bool material_refused;
};

bool forced(const Work& w) {
  const bool count_zero = (w.required_mask & 7u) == 0;
  const bool witness_ok = !count_zero ||
      ((w.desc.cls | w.desc.palette_slot | w.desc.palette_generation) == 0);
  const bool aux_ok = w.desc.aux_required || aux_zero(w.desc.aux);
  return w.material_refused ||
         descriptor_mask(w.desc) != w.required_mask || !witness_ok || !aux_ok;
}

struct SampleExpected {
  uint16_t handle;
  uint8_t page_generation, overflow, force, selector, lod;
  int32_t u, v;
  uint8_t cls, palette_slot, palette_generation;
};
struct MosaicExpected {
  uint16_t owner;
  int32_t u, v;
  uint8_t material_a, material_b, weight;
};
struct AuxExpected {
  uint16_t owner;
  Aux224 context;
  bool force;
};

void append_expected(const Work& w, std::deque<SampleExpected>& samples,
                     std::deque<MosaicExpected>& mosaics,
                     std::deque<AuxExpected>& auxes) {
  const bool force = forced(w);
  mosaics.push_back(MosaicExpected{w.owner, w.u, w.v,
                                   w.desc.material_a, w.desc.material_b,
                                   w.desc.mosaic_weight});
  for (unsigned sample = 0; sample < 3; ++sample) {
    if (((w.required_mask >> sample) & 1u) == 0) continue;
    const unsigned selector9 = static_cast<unsigned>(w.desc.selector) + sample;
    samples.push_back(SampleExpected{
        static_cast<uint16_t>((((w.owner >> 8) & 0x3Fu) << 10) |
                              (sample << 8) | (w.owner & 0x00FFu)),
        w.desc.page_generation, static_cast<uint8_t>((selector9 >> 8) & 1u),
        static_cast<uint8_t>(force), static_cast<uint8_t>(selector9), w.desc.lod,
        w.u, w.v, w.desc.cls, w.desc.palette_slot, w.desc.palette_generation});
  }
  if (w.required_mask & 8u) {
    AuxExpected a{w.owner, w.desc.aux, force};
    if (force) a.context.fill(0);
    auxes.push_back(a);
  }
}

bool aux_matches(ExpandDut* d, const Aux224& expected) {
  for (unsigned i = 0; i < 7; ++i)
    if (d->aux_context_o[i] != expected[i]) return false;
  return true;
}

void drive(ExpandDut* d, const Work& w) {
  d->frag_valid_i = 1;
  d->frag_owner_i = w.owner;
  const auto bits = pack(w.desc);
  for (unsigned i = 0; i < bits.size(); ++i)
    d->frag_logical_descriptor_i[i] = bits[i];
  d->frag_u_i = w.u;
  d->frag_v_i = w.v;
  d->frag_required_mask_i = w.required_mask;
  d->frag_material_refused_i = w.material_refused;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* d = new ExpandDut;
  d->clk = 0;
  d->rst_n = 0;
  d->frame_fault_clear_i = 0;
  d->frag_valid_i = 0;
  d->sample_ready_i = 0;
  d->mosaic_ready_i = 0;
  d->aux_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

#if defined(ZHAO_FRAG_EXPAND_QUEUE_MUTANT)
  Work trapped{};
  trapped.owner = 0x1234;
  trapped.desc.count = 1;
  trapped.desc.selector = 9;
  trapped.desc.page_generation = 1;
  trapped.required_mask = 1;
  trapped.material_refused = false;
  for (unsigned i = 0; i < 5; ++i) {
    trapped.owner = static_cast<uint16_t>(0x1200u + i);
    drive(d, trapped);
    d->eval();
    zhao::check(d->frag_ready_o,
                "QUEUE-GUARD MUTANT FIRE setup admitted the next trapped fragment",
                1, d->frag_ready_o);
    tick(d);
  }
  d->frag_valid_i = 0;
  tick(d);
  zhao::check(d->fragments_accepted_o == 5 && d->wq_overflow_o == 1,
              "QUEUE-GUARD MUTANT FIRE: weakened full guard made occupancy exceed four and counter fired",
              1, (d->fragments_accepted_o == 5 && d->wq_overflow_o == 1) ? 1 : 0);
  {
    const int rc = zhao::report_and_exit("frag_expand_v2_queue_guard_mutant");
    delete d;
    zhao::exit_hard(rc);
  }
#endif

  std::vector<Work> work;
  for (unsigned i = 0; i < 28; ++i) {
    Work w{};
    w.owner = static_cast<uint16_t>(((i * 5u) & 0x3Fu) << 8 |
                                    ((0x31u + i * 13u) & 0xFFu));
    w.u = static_cast<int32_t>(0x10000 + i * 0x123);
    w.v = -static_cast<int32_t>(0x20000 + i * 0x87);
    w.desc.lod = static_cast<uint8_t>(0x10u + i);
    w.desc.cls = static_cast<uint8_t>(i % 3u);
    w.desc.count = static_cast<uint8_t>(i & 3u);
    w.desc.aux_required = static_cast<uint8_t>((i % 5u) == 2u);
    w.desc.palette_slot = static_cast<uint8_t>(i & 3u);
    w.desc.palette_generation = static_cast<uint8_t>(0x50u + i);
    w.desc.material_a = static_cast<uint8_t>(i + 1u);
    w.desc.material_b = static_cast<uint8_t>(i + 33u);
    w.desc.mosaic_weight = static_cast<uint8_t>(255u - i);
    w.desc.selector = static_cast<uint8_t>(i == 7 ? 255u : 13u + i * 7u);
    w.desc.page_generation = static_cast<uint8_t>(1u + i);
    for (unsigned k = 0; k < 7; ++k)
      w.desc.aux[k] = w.desc.aux_required
                          ? (0xA5000000u | (i << 8) | k)
                          : 0u;
    w.required_mask = descriptor_mask(w.desc);
    w.material_refused = false;
    if (w.desc.count == 0) {
      w.desc.cls = 0;
      w.desc.palette_slot = 0;
      w.desc.palette_generation = 0;
    }
    work.push_back(w);
  }

  // Discriminating edge cases appended explicitly.
  Work mismatch = work[5];
  mismatch.owner = 0x2A91;
  mismatch.desc.count = 1;
  mismatch.desc.aux_required = 0;
  mismatch.desc.aux.fill(0);
  mismatch.required_mask = 3;  // owner also owes sample 1: descriptor disagrees
  work.push_back(mismatch);

  Work unusable{};
  unusable.owner = 0x17C4;
  unusable.u = 0x123456;
  unusable.v = -0x654321;
  // This is the descriptor bank's unusable-row representation: logical zero.
  // The nonempty owner mask still creates every owed, force-refused job.
  unusable.required_mask = 0x0C;  // sample 2 plus AUX, owner remains authority
  unusable.material_refused = false;
  work.push_back(unusable);

  Work material = work[6];
  material.owner = 0x35E2;
  material.material_refused = true;
  work.push_back(material);

  Work noncanonical = work[0];
  noncanonical.owner = 0x04F0;
  noncanonical.desc.count = 0;
  noncanonical.required_mask = 0;
  noncanonical.desc.cls = 2;  // count-zero witnesses must be zero
  work.push_back(noncanonical);

  std::deque<SampleExpected> samples;
  std::deque<MosaicExpected> mosaics;
  std::deque<AuxExpected> auxes;
  size_t next_input = 0;
  uint32_t rng = 0xC001D00Du;
  unsigned sample_errors = 0, mosaic_errors = 0, aux_errors = 0,
           issue_errors = 0;
  unsigned accepted = 0, observed_samples = 0, observed_mosaics = 0,
           observed_aux = 0;
  unsigned expected_zero = 0, expected_malformed = 0;

  for (unsigned cycle = 0; cycle < 20000; ++cycle) {
    rng = rng * 1664525u + 1013904223u;
    d->sample_ready_i = ((rng >> 5) & 3u) != 0;
    d->mosaic_ready_i = ((rng >> 8) & 3u) != 0;
    d->aux_ready_i = ((rng >> 11) & 1u) != 0;
    if (next_input < work.size()) drive(d, work[next_input]);
    else d->frag_valid_i = 0;
    d->eval();

    if (d->sample_valid_o) {
      if (samples.empty()) {
        ++sample_errors;
      } else {
        const auto& e = samples.front();
        if (d->sample_handle_o != e.handle ||
            d->sample_page_generation_o != e.page_generation ||
            d->sample_selector_overflow_o != e.overflow ||
            d->sample_force_refuse_o != e.force ||
            d->sample_binding_selector_o != e.selector ||
            static_cast<int32_t>(d->sample_u_o) != e.u ||
            static_cast<int32_t>(d->sample_v_o) != e.v ||
            d->sample_lod_q4_4_o != e.lod ||
            d->sample0_class_witness_o != e.cls ||
            d->sample0_palette_slot_witness_o != e.palette_slot ||
            d->sample0_palette_generation_witness_o != e.palette_generation)
          ++sample_errors;
      }
    }
    if (d->mosaic_valid_o) {
      if (mosaics.empty()) {
        ++mosaic_errors;
      } else {
        const auto& e = mosaics.front();
        if (d->mosaic_owner_o != e.owner ||
            static_cast<int32_t>(d->mosaic_u_o) != e.u ||
            static_cast<int32_t>(d->mosaic_v_o) != e.v ||
            d->mosaic_material_a_o != e.material_a ||
            d->mosaic_material_b_o != e.material_b ||
            d->mosaic_weight_o != e.weight)
          ++mosaic_errors;
      }
    }
    if (d->aux_valid_o) {
      if (auxes.empty()) {
        ++aux_errors;
      } else {
        const auto& e = auxes.front();
        if (d->aux_owner_o != e.owner || d->aux_force_refuse_o != e.force ||
            !aux_matches(d, e.context))
          ++aux_errors;
      }
    }

    const bool sample_fire = d->sample_valid_o && d->sample_ready_i;
    const bool mosaic_fire = d->mosaic_valid_o && d->mosaic_ready_i;
    const bool aux_fire = d->aux_valid_o && d->aux_ready_i;
    const bool frag_fire = d->frag_valid_i && d->frag_ready_o;
    if (sample_fire) {
      ++observed_samples;
      if (samples.empty()) ++sample_errors;
      else samples.pop_front();
    }
    if (mosaic_fire) {
      ++observed_mosaics;
      if (mosaics.empty()) ++mosaic_errors;
      else mosaics.pop_front();
    }
    if (aux_fire) {
      ++observed_aux;
      if (!d->iss_aux_valid_o || d->iss_aux_owner_o != d->aux_owner_o)
        ++issue_errors;
      if (auxes.empty()) ++aux_errors;
      else auxes.pop_front();
    } else if (d->iss_aux_valid_o) {
      ++issue_errors;
    }
    if (frag_fire) {
      const Work& w = work[next_input];
      append_expected(w, samples, mosaics, auxes);
      if ((w.required_mask & 7u) == 0) ++expected_zero;
      const bool descriptor_bad = forced(w) && !w.material_refused;
      if (descriptor_bad) ++expected_malformed;
      ++next_input;
      ++accepted;
    }

    tick(d);
    if (next_input == work.size() && samples.empty() && mosaics.empty() &&
        auxes.empty() && d->idle_o)
      break;
  }

  d->frag_valid_i = 0;
  d->eval();

  // Accepted malformed metadata faults even when the owner mask is empty. The
  // one mandatory Mosaic record keeps the queue non-idle and supplies a complete
  // hold check while frame state is cleared independently.
  {
    d->frame_fault_clear_i = 1;
    tick(d);
    d->frame_fault_clear_i = 0;
    d->eval();
    const uint32_t malformed_before = d->malformed_descriptors_o;
    const uint32_t samples_before = d->sample_jobs_accepted_o;
    const uint32_t aux_before = d->aux_jobs_accepted_o;
    Work bad_empty{};
    bad_empty.owner = 0x3A5C;
    bad_empty.u = 0x13572468;
    bad_empty.v = static_cast<int32_t>(0x89ABCDEFu);
    bad_empty.desc.count = 0;
    bad_empty.desc.cls = 2; // forbidden nonzero count-zero witness
    bad_empty.desc.material_a = 0xA1;
    bad_empty.desc.material_b = 0xB2;
    bad_empty.desc.mosaic_weight = 0xC3;
    bad_empty.required_mask = 0;
    bad_empty.material_refused = false;
    d->mosaic_ready_i = 0;
    d->frame_fault_clear_i = 1; // same edge: malformed set must win
    drive(d, bad_empty);
    d->eval();
    zhao::check(d->frag_ready_o, "empty-mask malformed control was accepted", 1,
                d->frag_ready_o);
    tick(d);
    d->frag_valid_i = 0;
    d->frame_fault_clear_i = 0;
    d->eval();
    zhao::check(d->frame_fault_o &&
                    d->malformed_descriptors_o == malformed_before + 1,
                "malformed empty-mask acceptance sets fault over same-edge clear and counts once",
                1, (d->frame_fault_o &&
                    d->malformed_descriptors_o == malformed_before + 1) ? 1 : 0);
    zhao::check(d->mosaic_valid_o && !d->sample_valid_o && !d->aux_valid_o &&
                    !d->idle_o,
                "Mosaic remains the explicit held obligation for an empty owner mask",
                1, (d->mosaic_valid_o && !d->sample_valid_o &&
                    !d->aux_valid_o && !d->idle_o) ? 1 : 0);
    const uint16_t held_owner = d->mosaic_owner_o;
    const int32_t held_u = static_cast<int32_t>(d->mosaic_u_o);
    const int32_t held_v = static_cast<int32_t>(d->mosaic_v_o);
    const uint8_t held_a = d->mosaic_material_a_o;
    const uint8_t held_b = d->mosaic_material_b_o;
    const uint8_t held_w = d->mosaic_weight_o;
    d->frame_fault_clear_i = 1;
    tick(d);
    d->frame_fault_clear_i = 0;
    d->eval();
    zhao::check(d->mosaic_valid_o && d->mosaic_owner_o == held_owner &&
                    static_cast<int32_t>(d->mosaic_u_o) == held_u &&
                    static_cast<int32_t>(d->mosaic_v_o) == held_v &&
                    d->mosaic_material_a_o == held_a &&
                    d->mosaic_material_b_o == held_b &&
                    d->mosaic_weight_o == held_w,
                "every Mosaic owner/U/V/A/B/weight bit holds through backpressure and clear",
                1, 1);
    zhao::check(!d->frame_fault_o &&
                    d->malformed_descriptors_o == malformed_before + 1 &&
                    d->sample_jobs_accepted_o == samples_before &&
                    d->aux_jobs_accepted_o == aux_before,
                "frame clear changes no malformed/sample/AUX counter or held work",
                1, (!d->frame_fault_o &&
                    d->malformed_descriptors_o == malformed_before + 1 &&
                    d->sample_jobs_accepted_o == samples_before &&
                    d->aux_jobs_accepted_o == aux_before) ? 1 : 0);
    d->mosaic_ready_i = 1;
    tick(d);
    d->mosaic_ready_i = 0;
    d->eval();
    ++accepted;
    ++observed_mosaics;
    ++expected_zero;
    ++expected_malformed;
  }

  std::printf("  fragments=%u samples=%u mosaic=%u aux=%u malformed=%u overflow=%u\n",
              accepted, observed_samples, observed_mosaics, observed_aux,
              d->malformed_descriptors_o, d->wq_overflow_o);

  zhao::check(next_input == work.size(),
              "every held fragment offer was eventually accepted", work.size(),
              next_input);
  zhao::check(samples.empty(),
              "every owner-required sample received one logical job", 0,
              samples.size());
  zhao::check(mosaics.empty(),
              "every accepted fragment emitted one owner/U/V/Mosaic record", 0,
              mosaics.size());
  zhao::check(auxes.empty(),
              "every owner-required AUX bit received a separate logical job", 0,
              auxes.size());
#if defined(ZHAO_FRAG_EXPAND_SELECTOR_MUTANT)
  zhao::check(sample_errors > 0,
              "SELECTOR-WRAP MUTANT FIRE: a base-255 sample lost its ninth carry bit",
              1, sample_errors > 0 ? 1 : 0);
#else
  zhao::check(sample_errors == 0,
              "every sample record holds owner/sample/page/selector/U/V/LOD/witness atomically",
              0, sample_errors);
#endif
  zhao::check(aux_errors == 0,
              "AUX retains its own owner and all 224 context bits and never substitutes for sample 2",
              0, aux_errors);
  zhao::check(mosaic_errors == 0,
              "Mosaic owner/U/V/material A/B/weight is bit-exact through every stall",
              0, mosaic_errors);
  zhao::check(issue_errors == 0,
              "AUX issue notification occurs exactly on its accepted job edge", 0,
              issue_errors);
  zhao::check(d->fragments_accepted_o == accepted,
              "fragment acceptance counter is exact", accepted,
              d->fragments_accepted_o);
  zhao::check(d->sample_jobs_accepted_o == observed_samples,
              "sample-job counter detects duplicate or missing expansion work",
              observed_samples, d->sample_jobs_accepted_o);
  zhao::check(d->mosaic_jobs_accepted_o == observed_mosaics,
              "Mosaic counter is exactly one accepted record per fragment",
              observed_mosaics, d->mosaic_jobs_accepted_o);
  zhao::check(d->aux_jobs_accepted_o == observed_aux,
              "AUX-job counter detects duplicate or missing work", observed_aux,
              d->aux_jobs_accepted_o);
  zhao::check(d->zero_sample_fragments_o == expected_zero,
              "count-zero is accepted and creates no implicit sample", expected_zero,
              d->zero_sample_fragments_o);
  zhao::check(d->malformed_descriptors_o == expected_malformed,
              "descriptor/mask and canonical-zero disagreements are independently counted",
              expected_malformed, d->malformed_descriptors_o);
  zhao::check(d->wq_overflow_o == 0,
              "legal backpressure never exceeds the fragment queue's owned depth", 0,
              d->wq_overflow_o);
  zhao::check(d->idle_o != 0, "all fragment/sample/Mosaic/AUX state drains", 1,
              d->idle_o);

  const int rc = zhao::report_and_exit("texture_frag_expand_v2_directed");
  delete d;
  zhao::exit_hard(rc);
}
