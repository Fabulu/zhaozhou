// texture_desc_expand_bind_v2_composed.cpp -- private three-leaf seam gate.
#include "Vtb_texture_desc_expand_bind_v2_composed.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>

#include "verilated.h"
#include "zref/zref_terrain.hpp"

#include "../harness/zhao_sim.hpp"

namespace {

using Dut = Vtb_texture_desc_expand_bind_v2_composed;

void tick(Dut* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

struct Row {
  uint32_t base = 0;
  uint32_t mode = 0;
  uint8_t palette_slot = 0;
  uint8_t palette_generation = 0;
  bool valid = true;
};

std::array<uint32_t, 3> pack_row(const Row& r) {
  return {r.base, r.mode,
          static_cast<uint32_t>(r.palette_slot & 3u) |
              (static_cast<uint32_t>(r.palette_generation) << 2) |
              (static_cast<uint32_t>(r.valid) << 10)};
}

uint32_t crc_byte(uint32_t crc, uint8_t data) {
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = ((crc ^ (data >> bit)) & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
  return crc;
}

uint32_t page_crc(uint8_t generation, const std::array<Row, 256>& rows,
                  const std::array<bool, 256>& present) {
  uint32_t crc = crc_byte(0xFFFFFFFFu, generation);
  for (unsigned selector = 0; selector < 256; ++selector) {
    std::array<uint8_t, 10> bytes{};
    if (present[selector]) {
      const auto packed = pack_row(rows[selector]);
      for (unsigned bit = 0; bit < 75; ++bit)
        if ((packed[bit / 32] >> (bit % 32)) & 1u)
          bytes[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
    for (uint8_t byte : bytes) crc = crc_byte(crc, byte);
  }
  return crc ^ 0xFFFFFFFFu;
}

uint32_t clut_mode() {
  // CLUT8, nearest, repeat U/V, 4x4, no mip.
  return (2u << 8) | (2u << 12);
}

// The TILESET row of terrain_rules 6.2 / zref::Tileset: CLUT8, unfiltered,
// MIRROR on both axes, 64x64, no mip chain, mode[21] set. Written out from the
// field positions rather than as a magic constant so it can be read against
// `binding_row_legal`'s own decode.
constexpr uint32_t kModeTilesetBit = 1u << 21;
uint32_t tileset_mode() {
  return (0u /* FMT_CLUT8 */) | (0u << 3 /* filter */) | (2u << 4 /* wrap_u mirror */) |
         (2u << 6 /* wrap_v mirror */) | (6u << 8 /* log2w */) | (6u << 12 /* log2h */) |
         (0u << 16 /* max_level */) | (0u << 20 /* mip_enable */) | kModeTilesetBit;
}

constexpr uint32_t kTilesetTileBytes = 64u * 64u;  // zref::Tileset tiles[256][64*64]

// The mosaic triple `write_descriptor` puts in the descriptor for a seed. Kept
// beside it so the two cannot drift.
uint8_t seed_mat_a(uint8_t seed) { return static_cast<uint8_t>(0xA0u + seed); }
uint8_t seed_mat_b(uint8_t seed) { return static_cast<uint8_t>(0xB0u + seed); }
uint8_t seed_weight(uint8_t seed) { return static_cast<uint8_t>(0xC0u + seed); }

// The ORACLE for the pick, and it is `zref`'s own frozen 6.2 function, not a
// transcription of the RTL's shift/add trees. The RTL reaches the same number
// through `mul_cx`/`mul_cy` and `zhao_texture_mod255`, which share no arithmetic
// with this call.
uint8_t expected_tile(uint8_t seed, int32_t u, int32_t v) {
  return zref::terrain::mosaic_pick(seed_mat_a(seed), seed_mat_b(seed), seed_weight(seed), u >> 10,
                                    v >> 10);
}

uint16_t owner(unsigned slot, unsigned generation) {
  return static_cast<uint16_t>(((slot & 0x3Fu) << 8) | (generation & 0xFFu));
}
uint16_t sample_handle(uint16_t owner_handle, unsigned sample) {
  return static_cast<uint16_t>((((owner_handle >> 8) & 0x3Fu) << 10) | ((sample & 3u) << 8) |
                               (owner_handle & 0x00FFu));
}

void clear_cfg_row(Dut* d) {
  for (unsigned i = 0; i < 3; ++i) d->cfg_row_i[i] = 0;
}
void drive_cfg_row(Dut* d, const Row& row) {
  const auto packed = pack_row(row);
  for (unsigned i = 0; i < 3; ++i) d->cfg_row_i[i] = packed[i];
}

void cfg_immediate(Dut* d, uint8_t op, uint8_t generation, uint8_t selector, const Row* row,
                   uint32_t crc) {
  d->cfg_op_i = op;
  d->cfg_page_generation_i = generation;
  d->cfg_selector_i = selector;
  if (row)
    drive_cfg_row(d, *row);
  else
    clear_cfg_row(d);
  d->cfg_crc32_i = crc;
  d->cfg_valid_i = 1;
  d->eval();
  zhao::check(d->cfg_ready_o, "composed config command accepted", 1, d->cfg_ready_o);
  tick(d);
  d->cfg_valid_i = 0;
  d->eval();
  zhao::check(d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0,
              "composed BEGIN/WRITE returns held OK", 1,
              (d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0) ? 1 : 0);
  d->cfg_rsp_ready_i = 1;
  tick(d);
  d->cfg_rsp_ready_i = 0;
}

void cfg_end(Dut* d, uint8_t generation, uint32_t crc) {
  d->cfg_op_i = 2;
  d->cfg_page_generation_i = generation;
  d->cfg_selector_i = 0;
  clear_cfg_row(d);
  d->cfg_crc32_i = crc;
  d->cfg_valid_i = 1;
  d->eval();
  zhao::check(d->cfg_ready_o, "composed END accepted", 1, d->cfg_ready_o);
  tick(d);
  d->cfg_valid_i = 0;
  unsigned wait = 0;
  while (!d->cfg_rsp_valid_o && wait < 4000) {
    tick(d);
    ++wait;
  }
  zhao::check(
      d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0 && d->active_page_generation_o == generation,
      "composed canonical CRC seals and activates requested generation", 1,
      (d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 0 && d->active_page_generation_o == generation)
          ? 1
          : 0);
  d->cfg_rsp_ready_i = 1;
  tick(d);
  d->cfg_rsp_ready_i = 0;
}

void write_descriptor(Dut* d, uint16_t owner_handle, uint8_t selector, uint8_t count,
                      uint8_t page_generation, uint8_t material_seed) {
  d->desc_wr_valid_i = 1;
  d->desc_wr_slot_i = static_cast<uint8_t>(owner_handle >> 8);
  d->desc_wr_owner_generation_i = static_cast<uint8_t>(owner_handle);
  for (unsigned i = 0; i < 7; ++i) d->desc_wr_aux_context_i[i] = 0;
  d->desc_wr_lod_q4_4_i = static_cast<uint8_t>(0x20u + material_seed);
  d->desc_wr_response_class_i = 0;
  d->desc_wr_aux_required_i = 0;
  d->desc_wr_sample_count_i = count;
  d->desc_wr_palette_slot_i = count ? 2 : 0;
  d->desc_wr_palette_generation_i = count ? 0x55 : 0;
  d->desc_wr_mosaic_material_a_i = static_cast<uint8_t>(0xA0u + material_seed);
  d->desc_wr_mosaic_material_b_i = static_cast<uint8_t>(0xB0u + material_seed);
  d->desc_wr_mosaic_weight_i = static_cast<uint8_t>(0xC0u + material_seed);
  d->desc_wr_binding_selector_i = selector;
  d->desc_wr_active_page_generation_i = page_generation;
  tick(d);
  d->desc_wr_valid_i = 0;
}

struct PlanExpected {
  uint16_t handle;
  uint32_t base, mode;
  int32_t u, v;
  uint8_t lod;
};

struct Stats {
  unsigned issues = 0;
  unsigned plans = 0;
  unsigned refusals = 0;
  unsigned errors = 0;
  uint32_t rng = 0x1234ABCDu;
  std::deque<PlanExpected> expected_plans;
  std::deque<uint16_t> expected_refusals;
};

void observe_tick(Dut* d, Stats& s) {
  s.rng = s.rng * 1664525u + 1013904223u;
  d->plan_ready_i = ((s.rng >> 7) & 3u) != 0;
  d->refuse_ready_i = ((s.rng >> 13) & 1u) != 0;
  d->eval();
  if (d->issue_valid_o) ++s.issues;
  if (d->plan_valid_o && d->plan_ready_i) {
    ++s.plans;
    if (s.expected_plans.empty()) {
      ++s.errors;
    } else {
      const auto e = s.expected_plans.front();
      s.expected_plans.pop_front();
      if ((d->plan_route_token_o & 0xFFFFu) != e.handle || (d->plan_route_token_o >> 16) != 0 ||
          d->plan_base_o != e.base || d->plan_mode_o != e.mode || d->plan_palette_slot_o != 2 ||
          d->plan_palette_generation_o != 0x55 || static_cast<int32_t>(d->plan_u_o) != e.u ||
          static_cast<int32_t>(d->plan_v_o) != e.v || d->plan_lod_q4_4_o != e.lod)
        ++s.errors;
    }
  }
  if (d->refuse_valid_o && d->refuse_ready_i) {
    ++s.refusals;
    if (s.expected_refusals.empty()) {
      ++s.errors;
    } else {
      const uint16_t expected = s.expected_refusals.front();
      s.expected_refusals.pop_front();
      if (d->refuse_handle_o != expected || d->refuse_result_o != 0x0100FFFF00FFull) ++s.errors;
    }
  }
  tick(d);
}

void submit_and_drain(Dut* d, Stats& stats, uint16_t owner_handle, uint8_t selector, uint8_t count,
                      int32_t u, int32_t v, const std::array<Row, 256>& rows,
                      uint8_t page_generation, uint8_t seed) {
  write_descriptor(d, owner_handle, selector, count, page_generation, seed);
  const uint8_t lod = static_cast<uint8_t>(0x20u + seed);
  for (unsigned sample = 0; sample < count; ++sample) {
    const unsigned selector9 = static_cast<unsigned>(selector) + sample;
    const uint16_t handle = sample_handle(owner_handle, sample);
    if (selector9 < 256) {
      // THE READER'S LAW, stated here independently of the RTL: a TILESET row's
      // base is displaced by the mosaic's picked tile; every other row is not.
      uint32_t base = rows[selector9].base;
      if (rows[selector9].mode & kModeTilesetBit)
        base += static_cast<uint32_t>(expected_tile(seed, u, v)) * kTilesetTileBytes;
      stats.expected_plans.push_back(
          PlanExpected{handle, base, rows[selector9].mode, u, v, lod});
    } else {
      stats.expected_refusals.push_back(handle);
    }
  }

  d->frag_owner_i = owner_handle;
  d->frag_u_i = u;
  d->frag_v_i = v;
  d->frag_required_mask_i = count == 1 ? 1 : count == 2 ? 3 : 7;
  d->frag_material_refused_i = 0;
  d->frag_valid_i = 1;
  unsigned admission_wait = 0;
  d->eval();
  while (!d->frag_ready_o && admission_wait < 500) {
    d->plan_ready_i = 0;
    d->refuse_ready_i = 0;
    tick(d);
    ++admission_wait;
  }
  zhao::check(d->frag_ready_o, "composed fragment admission is bounded", 1, d->frag_ready_o);
  if (d->frag_ready_o) tick(d);
  d->frag_valid_i = 0;

  unsigned wait = 0;
  while (!d->quiet_o && wait < 500) {
    observe_tick(d, stats);
    ++wait;
  }
  zhao::check(d->quiet_o, "composed fragment fully drained", 1, d->quiet_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* d = new Dut;
  d->clk = 0;
  d->rst_n = 0;
  d->frame_fault_clear_i = 0;
  d->desc_wr_valid_i = 0;
  d->frag_valid_i = 0;
  d->cfg_valid_i = 0;
  d->cfg_rsp_ready_i = 0;
  d->plan_ready_i = 0;
  d->refuse_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  std::array<Row, 256> rows{};
  std::array<bool, 256> present{};
  for (unsigned selector : {0u, 1u, 253u, 254u, 255u}) {
    rows[selector] = Row{0x00100000u + selector * 0x1000u, clut_mode(), 2, 0x55, true};
    present[selector] = true;
  }

#if defined(ZHAO_COMPOSED_PAD_MUTANT)
  Stats stats;
  const uint16_t h = owner(3, 0x41);
  write_descriptor(d, h, 5, 1, 9, 1);
  stats.expected_refusals.push_back(sample_handle(h, 0));
  d->frag_owner_i = h;
  d->frag_u_i = 0x12340000;
  d->frag_v_i = static_cast<int32_t>(0xFEDC0000u);
  d->frag_required_mask_i = 1;
  d->frag_material_refused_i = 0;
  d->frag_valid_i = 1;
  d->eval();
  zhao::check(d->frag_ready_o, "bad-pad composed fragment accepted", 1, d->frag_ready_o);
  tick(d);
  d->frag_valid_i = 0;
  unsigned wait = 0;
  while (!d->refuse_valid_o && wait < 100) {
    d->plan_ready_i = 1;
    d->refuse_ready_i = 0;
    d->eval();
    if (d->issue_valid_o) ++stats.issues;
    tick(d);
    ++wait;
  }
  zhao::check(d->refuse_valid_o, "bad pad reaches terminal local refusal", 1, d->refuse_valid_o);
  d->plan_ready_i = 1;
  d->refuse_ready_i = 0;
  const uint32_t pad_count = d->desc_pad_fault_o;
  const uint16_t held_handle = d->refuse_handle_o;
  const uint64_t held_result = d->refuse_result_o;
  for (unsigned hold = 0; hold < 4; ++hold) {
    d->eval();
    if (d->issue_valid_o) ++stats.issues;
    zhao::check(d->refuse_valid_o && d->refuse_handle_o == held_handle &&
                    d->refuse_result_o == held_result && d->desc_pad_fault_o == pad_count,
                "bad-pad terminal payload and one-shot counter hold under stall", 1, 1);
    tick(d);
  }
  zhao::check(d->desc_pad_fault_o == pad_count && pad_count == 1,
              "held bad-pad refusal cannot recount descriptor fault", 1,
              (d->desc_pad_fault_o == 1) ? 1 : 0);
  d->refuse_ready_i = 1;
  d->eval();
  if (d->issue_valid_o) ++stats.issues;
  if (d->refuse_valid_o) {
    ++stats.refusals;
    if (stats.expected_refusals.empty() || d->refuse_handle_o != stats.expected_refusals.front() ||
        d->refuse_result_o != 0x0100FFFF00FFull)
      ++stats.errors;
    else
      stats.expected_refusals.pop_front();
  }
  tick(d);
  d->refuse_ready_i = 0;
  d->eval();
  zhao::check(d->quiet_o, "bad-pad composed path drains after terminal acceptance", 1, d->quiet_o);
  zhao::check(
      stats.issues == 1 && stats.plans == 0 && stats.refusals == 1 && stats.errors == 0,
      "bad pad produces exactly one issue, zero planner work, one typed terminal refusal", 1,
      (stats.issues == 1 && stats.plans == 0 && stats.refusals == 1 && stats.errors == 0) ? 1 : 0);
  zhao::check(d->desc_frame_fault_o && d->expand_frame_fault_o && d->resolver_frame_fault_o &&
                  d->expand_malformed_o == 1 && d->resolver_page_mismatch_o == 1,
              "bad pad independently faults descriptor, expander, and resolver generation verdict",
              1,
              (d->desc_frame_fault_o && d->expand_frame_fault_o && d->resolver_frame_fault_o &&
               d->expand_malformed_o == 1 && d->resolver_page_mismatch_o == 1)
                  ? 1
                  : 0);
#else
  cfg_immediate(d, 0, 7, 0, nullptr, 0);
  for (unsigned selector : {255u, 0u, 254u, 1u, 253u})
    cfg_immediate(d, 1, 7, static_cast<uint8_t>(selector), &rows[selector], 0);
  cfg_end(d, 7, page_crc(7, rows, present));

  Stats stats;
  submit_and_drain(d, stats, owner(4, 0x51), 253, 1, 0x10101010, -0x01010101, rows, 7, 1);
  submit_and_drain(d, stats, owner(5, 0x52), 254, 2, 0x20202020, -0x02020202, rows, 7, 2);
  submit_and_drain(d, stats, owner(6, 0x53), 255, 3, 0x30303030, -0x03030303, rows, 7, 3);

  zhao::check(stats.issues == 6 && stats.plans == 4 && stats.refusals == 2 && stats.errors == 0 &&
                  stats.expected_plans.empty() && stats.expected_refusals.empty(),
              "selectors 253/254/255 at counts 1/2/3 close exact issue/planner/refusal records", 1,
              (stats.issues == 6 && stats.plans == 4 && stats.refusals == 2 && stats.errors == 0 &&
               stats.expected_plans.empty() && stats.expected_refusals.empty())
                  ? 1
                  : 0);
  zhao::check(d->expand_fragments_o == 3 && d->expand_samples_o == 6 && d->expand_mosaics_o == 3 &&
                  d->resolver_samples_o == 6 && d->resolver_plans_o == 4 &&
                  d->resolver_refusals_o == 2 && d->resolver_overflow_o == 2,
              "composed counters prove one Mosaic per fragment and no wrapped selector 0/1 lookup",
              1,
              (d->expand_fragments_o == 3 && d->expand_samples_o == 6 && d->expand_mosaics_o == 3 &&
               d->resolver_samples_o == 6 && d->resolver_plans_o == 4 &&
               d->resolver_refusals_o == 2 && d->resolver_overflow_o == 2)
                  ? 1
                  : 0);
  zhao::check(
      d->desc_pad_fault_o == 0 && d->expand_malformed_o == 0 && d->resolver_page_mismatch_o == 0,
      "legal composed selector workload has no pad/descriptor/page fault", 0,
      static_cast<uint64_t>(d->desc_pad_fault_o || d->expand_malformed_o ||
                            d->resolver_page_mismatch_o));

  // ==========================================================================
  // THE MOSAIC PICK'S READER (TERRAINTEX, 2026-09-26)
  //
  // Until this commit zhao_texture_mosaic_v2's answer had no reader anywhere:
  // pick_tile_o landed on a wire that occurred twice in the island and was
  // consumed nowhere. Everything below is about the answer ARRIVING at an
  // address, and the oracle for it is zref::terrain::mosaic_pick -- the frozen
  // 6.2 function -- which shares no arithmetic with the RTL's CSD trees.
  // ==========================================================================
  zhao::check(d->resolver_tileset_samples_o == 0,
              "no tileset row is bound yet, so no sample has been displaced", 0,
              d->resolver_tileset_samples_o);
  const uint32_t picks_before = d->mosaic_picks_held_o;
  zhao::check(picks_before == 3, "one mosaic pick was held for each of the three fragments", 3,
              picks_before);

  // ---- 1. the tileset row's LEGALITY LAW, both polarities ------------------
  // A row that declares TILESET and is not the object 6.2 describes must be
  // refused AT WRITE, so a wrong fold or a mip chain can never be sampled.
  cfg_immediate(d, 0, 9, 0, nullptr, 0);
  {
    struct Bad {
      const char* what;
      uint32_t mode;
    };
    const Bad bad[] = {
        {"tileset row declaring RGB565 is refused", (tileset_mode() & ~7u) | 1u},
        {"tileset row declaring a filter is refused", tileset_mode() | (1u << 3)},
        {"tileset row wrapping REPEAT in u is refused", (tileset_mode() & ~(3u << 4))},
        {"tileset row wrapping CLAMP in v is refused",
         (tileset_mode() & ~(3u << 6)) | (1u << 6)},
        {"tileset row at 32x64 is refused", (tileset_mode() & ~(0xFu << 8)) | (5u << 8)},
        {"tileset row with a mip chain is refused",
         tileset_mode() | (1u << 20) | (1u << 16)},
    };
    for (const auto& b : bad) {
      Row row{0x00400000u, b.mode, 2, 0x55, true};
      d->cfg_valid_i = 1;
      d->cfg_op_i = 1;
      d->cfg_page_generation_i = 9;
      d->cfg_selector_i = 40;
      drive_cfg_row(d, row);
      d->cfg_crc32_i = 0;
      d->eval();
      tick(d);
      d->cfg_valid_i = 0;
      clear_cfg_row(d);
      d->cfg_rsp_ready_i = 1;
      unsigned w = 0;
      d->eval();
      while (!d->cfg_rsp_valid_o && w < 50) {
        tick(d);
        d->eval();
        ++w;
      }
      zhao::check(d->cfg_rsp_valid_o && d->cfg_rsp_status_o == 3, b.what, 3,
                  d->cfg_rsp_valid_o ? d->cfg_rsp_status_o : 0xFFu);
      tick(d);
      d->cfg_rsp_ready_i = 0;
    }
  }
  cfg_immediate(d, 3, 9, 0, nullptr, 0);  // ABORT the probe page

  // ---- 2. a LEGAL tileset page, and the pick reaching the address ----------
  std::array<Row, 256> trows{};
  std::array<bool, 256> tpresent{};
  // Selector 10 is the tileset; selector 11 beside it is an ORDINARY CLUT row
  // at the same base, and it is the negative control for every check below:
  // the pick is computed for it too and must not move its address by one byte.
  trows[10] = Row{0x00400000u, tileset_mode(), 2, 0x55, true};
  tpresent[10] = true;
  trows[11] = Row{0x00400000u, clut_mode(), 2, 0x55, true};
  tpresent[11] = true;
  cfg_immediate(d, 0, 11, 0, nullptr, 0);
  for (unsigned selector : {10u, 11u})
    cfg_immediate(d, 1, 11, static_cast<uint8_t>(selector), &trows[selector], 0);
  cfg_end(d, 11, page_crc(11, trows, tpresent));

  // The coordinates are chosen so the picked tile is NOT zero and the three do
  // not all agree -- a displacement of zero would pass against an unwired
  // reader, which is this repository's own "rung centre" lesson.
  struct TCase {
    unsigned slot;
    uint8_t generation;
    uint8_t seed;
    int32_t u;
    int32_t v;
  };
  const TCase tcases[] = {
      {20, 0x61, 4, 0x00080000, 0x00040000},
      {21, 0x62, 5, 0x00130000, static_cast<int32_t>(0xFFF90000u)},
      {22, 0x63, 6, 0x00210000, 0x00370000},
  };
  {
    uint8_t seen[3];
    unsigned nonzero = 0;
    for (unsigned i = 0; i < 3; ++i) {
      seen[i] = expected_tile(tcases[i].seed, tcases[i].u, tcases[i].v);
      if (seen[i] != 0) ++nonzero;
    }
    const bool ok = (nonzero == 3) && !(seen[0] == seen[1] && seen[1] == seen[2]);
    zhao::check(ok,
                "the three picks are non-zero and not all equal, so a dead reader cannot pass", 1,
                ok ? 1 : 0);
    std::printf("  mosaic picks: %u %u %u\n", seen[0], seen[1], seen[2]);
  }

  Stats tstats;
  for (const auto& t : tcases)
    submit_and_drain(d, tstats, owner(t.slot, t.generation), 10, 1, t.u, t.v, trows, 11, t.seed);
  for (const auto& t : tcases)
    submit_and_drain(d, tstats, owner(t.slot, static_cast<uint8_t>(t.generation + 0x10)), 11, 1,
                     t.u, t.v, trows, 11, t.seed);

  zhao::check(tstats.errors == 0 && tstats.plans == 6 && tstats.refusals == 0 &&
                  tstats.expected_plans.empty(),
              "every planner base matches zref::terrain::mosaic_pick on the tileset row and is "
              "untouched on the plain row",
              1,
              (tstats.errors == 0 && tstats.plans == 6 && tstats.refusals == 0 &&
               tstats.expected_plans.empty())
                  ? 1
                  : 0);
  zhao::check(d->resolver_tileset_samples_o == 3,
              "the tileset census counts the three tileset samples and not the three plain ones", 3,
              d->resolver_tileset_samples_o);
  zhao::check(d->mosaic_picks_held_o == picks_before + 6,
              "one pick was held for each of the six new fragments", picks_before + 6,
              d->mosaic_picks_held_o);
  // THE HOLD'S POSITIVE CONTROL, by legal stimulus. The second pass reuses the
  // same three SLOTS with new generations, so each of its samples is offered
  // while the slot still holds the previous generation's pick. That is exactly
  // the swap the seal exists to refuse, and a non-zero count proves the seal
  // was doing work rather than that the state was unreachable.
  zhao::check(d->mosaic_stale_slot_holds_o > 0,
              "the generation seal observed a slot holding somebody else's pick", 1,
              (d->mosaic_stale_slot_holds_o > 0) ? 1 : 0);
  std::printf("  mosaic: picks_held=%u stale_slot_holds=%u tileset_samples=%u\n",
              d->mosaic_picks_held_o, d->mosaic_stale_slot_holds_o,
              d->resolver_tileset_samples_o);
  stats.issues += tstats.issues;
  stats.plans += tstats.plans;
  stats.refusals += tstats.refusals;
#endif

  std::printf("  composed: issue=%u plan=%u refuse=%u pad=%u overflow=%u\n", stats.issues,
              stats.plans, stats.refusals, d->desc_pad_fault_o, d->resolver_overflow_o);
  const int rc = zhao::report_and_exit("texture_desc_expand_bind_v2_composed");
  delete d;
  zhao::exit_hard(rc);
}
