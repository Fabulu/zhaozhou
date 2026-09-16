// raster_texture_stage_v3_directed.cpp -- private Packet-C composition gate.
//
// Drives the committed 490-bit package layout through zhao_skid2, the exact
// Packet-C stage, one Packet-B V3 island, and the real RASTER.FRAGMENT leaf.
// The tile memory is an external fixed-one-cycle model.  Every admitted owner
// is independently scoreboarding sequence32, all continuation128 bits, and the
// complete result48 before zref::FragmentPipeline checks each physical write.

#include "Vtb_raster_texture_stage_v3.h"
#include "verilated.h"

#include "zref/zref_fragment.hpp"
#include "zref/zref_tilestore.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

namespace {

using Wide490 = std::array<uint32_t, 16>;

uint64_t g_cycle = 0;
uint32_t g_checks = 0;
uint32_t g_tests = 0;

[[noreturn]] void fail(const char* what) {
  std::fprintf(stderr, "packet-c directed FAIL at cycle %llu: %s\n",
               static_cast<unsigned long long>(g_cycle), what);
  std::abort();
}

void require(bool condition, const char* what) {
  if (!condition) fail(what);
  ++g_checks;
}

void begin_test(const char* name) {
  ++g_tests;
  std::printf("packet-c test %u: %s\n", g_tests, name);
}

void set_bits(Wide490& value, unsigned lo, unsigned width, uint32_t bits) {
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned at = lo + bit;
    const uint32_t mask = uint32_t{1} << (at & 31u);
    if ((bits >> bit) & 1u)
      value[at >> 5] |= mask;
    else
      value[at >> 5] &= ~mask;
  }
}

bool wide_equal_port(const Wide490& expected, const VlWide<16>& got) {
  for (unsigned word = 0; word < expected.size(); ++word)
    if (expected[word] != got[word]) return false;
  return true;
}

void drive_wide(VlWide<16>& port, const Wide490& value) {
  for (unsigned word = 0; word < value.size(); ++word) port[word] = value[word];
}

uint32_t crc32_byte(uint32_t crc, uint8_t value) {
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = ((crc ^ (value >> bit)) & 1u) ? ((crc >> 1) ^ 0xedb88320u) : (crc >> 1);
  return crc;
}

struct BindingRow {
  uint32_t base = 0;
  uint32_t mode = 0;
  uint8_t palette_slot = 0;
  uint8_t palette_generation = 0;
  bool valid = false;
};

std::array<uint8_t, 10> row_bytes(const BindingRow& row, bool present) {
  std::array<uint8_t, 10> bytes{};
  if (!present) return bytes;
  bytes[0] = static_cast<uint8_t>(row.base);
  bytes[1] = static_cast<uint8_t>(row.base >> 8);
  bytes[2] = static_cast<uint8_t>(row.base >> 16);
  bytes[3] = static_cast<uint8_t>(row.base >> 24);
  bytes[4] = static_cast<uint8_t>(row.mode);
  bytes[5] = static_cast<uint8_t>(row.mode >> 8);
  bytes[6] = static_cast<uint8_t>(row.mode >> 16);
  bytes[7] = static_cast<uint8_t>(row.mode >> 24);
  const uint16_t high = static_cast<uint16_t>(row.palette_slot & 3u) |
                        (static_cast<uint16_t>(row.palette_generation) << 2) |
                        (static_cast<uint16_t>(row.valid ? 1u : 0u) << 10);
  bytes[8] = static_cast<uint8_t>(high);
  bytes[9] = static_cast<uint8_t>(high >> 8);
  return bytes;
}

uint32_t binding_crc(uint8_t generation, const std::array<BindingRow, 256>& rows,
                     const std::array<bool, 256>& present) {
  uint32_t crc = crc32_byte(0xffffffffu, generation);
  for (unsigned selector = 0; selector < 256; ++selector)
    for (const uint8_t byte : row_bytes(rows[selector], present[selector]))
      crc = crc32_byte(crc, byte);
  return crc ^ 0xffffffffu;
}

struct Candidate {
  Wide490 packed{};
  uint8_t addr = 0;
  uint32_t depth = 0;
  uint32_t state = 0;
  uint16_t source = 0;
  uint32_t vertex_rgb = 0;
  uint8_t vertex_alpha = 0;
  uint8_t effect_tag = 0;
  uint8_t stencil_ref = 0;
  uint32_t result_rgb = 0;
  uint8_t result_alpha = 0;
  uint8_t result_index = 0;
  uint8_t result_status = 0;
};

Candidate make_candidate(uint8_t addr, uint32_t state, uint16_t source, uint32_t vertex_rgb,
                         uint8_t vertex_alpha, uint8_t effect_tag, uint8_t stencil_ref,
                         uint8_t sample_count, uint8_t binding, uint8_t recipe, uint32_t base_rgb,
                         uint8_t base_alpha, uint32_t result_rgb, uint8_t result_alpha,
                         uint8_t result_index, uint8_t result_status) {
  Candidate c;
  c.addr = addr;
  c.depth = 0x800000u | addr;
  c.state = state;
  c.source = source;
  c.vertex_rgb = vertex_rgb & 0xffffffu;
  c.vertex_alpha = vertex_alpha;
  c.effect_tag = effect_tag;
  c.stencil_ref = stencil_ref;
  c.result_rgb = result_rgb & 0xffffffu;
  c.result_alpha = result_alpha;
  c.result_index = result_index;
  c.result_status = result_status;

  // zhao_render_texture_pkg named spans, low field first.  AUX is canonical
  // zero because these Packet-C vectors do not request Surface Sheet work.
  set_bits(c.packed, 0, 8, sample_count ? 1u : 0u);   // palette generation
  set_bits(c.packed, 8, 2, 0);                        // palette slot
  set_bits(c.packed, 10, 2, sample_count ? 0u : 0u);  // response class
  set_bits(c.packed, 12, 8, base_alpha);
  set_bits(c.packed, 20, 24, base_rgb);
  set_bits(c.packed, 268, 1, 0);      // AUX absent
  set_bits(c.packed, 269, 8, 0x5au);  // distinct recipe weight
  set_bits(c.packed, 277, 3, recipe);
  set_bits(c.packed, 280, 8, 0x13u);  // Q4.4 LOD
  set_bits(c.packed, 288, 8, binding);
  set_bits(c.packed, 296, 2, sample_count);
  set_bits(c.packed, 298, 32, 0);  // v_over_w
  set_bits(c.packed, 330, 32, 0);  // u_over_w
  set_bits(c.packed, 362, 8, stencil_ref);
  set_bits(c.packed, 370, 8, effect_tag);
  set_bits(c.packed, 378, 8, vertex_alpha);
  set_bits(c.packed, 386, 24, vertex_rgb);
  set_bits(c.packed, 410, 16, source);
  set_bits(c.packed, 426, 32, state);
  set_bits(c.packed, 458, 24, c.depth);
  set_bits(c.packed, 482, 8, addr);
  return c;
}

Candidate make_count0(uint8_t addr, uint32_t state = 0) {
  const uint32_t base =
      0x310000u | (static_cast<uint32_t>(addr) << 8) | (static_cast<uint32_t>(addr) ^ 0x5au);
  const uint8_t alpha = static_cast<uint8_t>(0x80u | (addr & 0x3fu));
  return make_candidate(addr, state, static_cast<uint16_t>(0x7000u | addr),
                        0x120000u | (static_cast<uint32_t>(addr) << 8) | 0x34u, alpha,
                        static_cast<uint8_t>(0xa0u ^ addr), static_cast<uint8_t>(0x50u ^ addr), 0,
                        0, 0, base, alpha, base, alpha, 0, 0);
}

struct OwnerExpected {
  uint32_t sequence = 0;
  Candidate candidate{};
};

struct FragBeat {
  uint8_t addr = 0;
  uint32_t depth = 0;
  uint32_t state = 0;
  uint16_t source = 0;
  uint32_t vertex_rgb = 0;
  uint8_t vertex_alpha = 0;
  uint8_t effect_tag = 0;
  uint8_t stencil_ref = 0;
  uint32_t texture_rgb = 0;
  uint8_t texture_alpha = 0;
  uint8_t texture_index = 0;
  uint8_t status = 0;

  bool operator==(const FragBeat& other) const {
    return addr == other.addr && depth == other.depth && state == other.state &&
           source == other.source && vertex_rgb == other.vertex_rgb &&
           vertex_alpha == other.vertex_alpha && effect_tag == other.effect_tag &&
           stencil_ref == other.stencil_ref && texture_rgb == other.texture_rgb &&
           texture_alpha == other.texture_alpha && texture_index == other.texture_index &&
           status == other.status;
  }
};

struct Harness {
  VerilatedContext context;
  Vtb_raster_texture_stage_v3 dut{&context};
  std::array<uint64_t, 256> tile{};
  std::array<uint64_t, 256> oracle_tile{};
  std::deque<Candidate> skid_score;
  std::deque<OwnerExpected> owner_score;
  std::deque<std::pair<uint8_t, uint64_t>> expected_writes;
  std::vector<FragBeat> fragment_beats;
  std::vector<uint64_t> cand_fire_cycles;
  uint32_t stage_accepts = 0;
  uint32_t fragment_accepts = 0;
  uint32_t drops = 0;
  uint32_t physical_writes = 0;
  uint32_t skid_cancel_events = 0;
  bool identity_drop_seen = false;
  std::vector<uint8_t> accepted_tile_reads;
  bool score_inputs = true;

  bool fill_active = false;
  uint32_t fill_line = 0;
  unsigned fill_beat = 0;
  unsigned fill_delay = 0;
  unsigned next_fill_delay = 0;

  bool tile_rsp_valid = false;
  uint64_t tile_rsp_data = 0;

  Harness() {
    dut.clk = 0;
    dut.rst_n = 0;
    dut.skid_rst_n_i = 0;
    dut.in_valid_i = 0;
    dut.stage_admit_enable_i = 1;
    dut.fragment_pause_i = 0;
    dut.skid_cancel_i = 0;
    dut.frame_fault_clear_valid_i = 0;
    dut.cfg_valid_i = 0;
    dut.cfg_rsp_ready_i = 1;
    dut.fill_req_ready_i = 1;
    dut.fill_data_valid_i = 0;
    dut.fill_data_i = 0;
    dut.fill_refused_i = 0;
    dut.pal_load_valid_i = 0;
    dut.sheet_req_ready_i = 1;
    dut.pg_valid_i = 0;
    dut.pg_op_i = 1;
    dut.pg_status_i = 0;
    dut.pg_tag_i = 0;
    dut.pg_strength_i = 0;
    dut.pg_src_id_i = 0;
    dut.tile_rd_ready_i = 1;
    dut.tile_rd_valid_i = 0;
    dut.tile_rd_data_i = 0;
    dut.tile_wr_ready_i = 1;
    dut.classify_i = 0;
    Wide490 zero{};
    drive_wide(dut.in_data_i, zero);
  }

  FragBeat observed_fragment() const {
    FragBeat f;
    f.addr = dut.obs_frag_addr_o;
    f.depth = dut.obs_frag_depth_o;
    f.state = dut.obs_frag_state_o;
    f.source = dut.obs_frag_src_id_o;
    f.vertex_rgb = dut.obs_frag_vert_rgb_o;
    f.vertex_alpha = dut.obs_frag_vert_a_o;
    f.effect_tag = dut.obs_frag_tag_o;
    f.stencil_ref = dut.obs_frag_sten_ref_o;
    f.texture_rgb = dut.obs_frag_texel_rgb_o;
    f.texture_alpha = dut.obs_frag_texel_a_o;
    f.texture_index = dut.obs_frag_texel_idx_o;
    f.status = dut.obs_frag_status_o;
    return f;
  }

  uint16_t fill_word(uint32_t line, unsigned beat) const {
    if (beat != 0) return 0;
    if (line == 0x00002000u) return 0x0005u;  // raw CLUT8 index 5
    if (line == 0x00003000u) return 0x0000u;  // raw CLUT8 index 0
    return 0x00c7u;
  }

  struct Events {
    bool in_fire = false;
    bool cand_fire = false;
    bool fragment_fire = false;
    bool drop_fire = false;
    bool mismatch = false;
    bool fill_req_fire = false;
    bool tile_rd_fire = false;
    bool tile_wr_fire = false;
    bool release = false;
    bool publish = false;
    bool skid_cancelled = false;
  };

  Events step() {
    dut.fill_data_valid_i = fill_active && (fill_delay == 0);
    dut.fill_data_i = dut.fill_data_valid_i ? fill_word(fill_line, fill_beat) : 0;
    dut.tile_rd_valid_i = tile_rsp_valid;
    dut.tile_rd_data_i = tile_rsp_data;

    dut.clk = 0;
    dut.eval();

    Events e;
    e.in_fire = dut.in_valid_i && dut.in_ready_o;
    e.cand_fire = dut.cand_fire_o;
    e.fragment_fire = dut.fragment_fire_o;
    e.drop_fire = dut.drop_fire_o;
    e.mismatch = dut.sequence_mismatch_o;
    e.fill_req_fire = dut.fill_req_valid_o && dut.fill_req_ready_i;
    e.tile_rd_fire = dut.tile_rd_valid_o && dut.tile_rd_ready_i;
    e.tile_wr_fire = dut.tile_wr_valid_o && dut.tile_wr_ready_i;
    e.release = dut.synthetic_release_o;
    e.publish = dut.synthetic_publish_o;
    e.skid_cancelled = dut.skid_cancelled_o;
    if (e.skid_cancelled) ++skid_cancel_events;

    if (e.in_fire && score_inputs) {
      Candidate accepted;
      accepted.packed = {};
      for (unsigned word = 0; word < accepted.packed.size(); ++word)
        accepted.packed[word] = dut.in_data_i[word];
      // offer() replaces this placeholder with the complete typed expectation
      // immediately after the accepted edge.
      skid_score.push_back(accepted);
    }

    if (e.cand_fire) {
      require(!skid_score.empty(), "stage admitted a candidate absent from skid scoreboard");
      require(wide_equal_port(skid_score.front().packed, dut.obs_cand_data_o),
              "stage candidate differed from independently held skid packet");
      OwnerExpected owner;
      owner.sequence = dut.admission_sequence_o;
      owner.candidate = skid_score.front();
      owner_score.push_back(owner);
      skid_score.pop_front();
      ++stage_accepts;
      cand_fire_cycles.push_back(g_cycle);
    }

    if (e.fragment_fire) {
      require(!owner_score.empty(), "fragment retired without an admitted owner");
      const OwnerExpected owner = owner_score.front();
      require(dut.returned_sequence_o == owner.sequence,
              "returned sequence did not match independent admission scoreboard");
      const Candidate& c = owner.candidate;
      const FragBeat f = observed_fragment();
      require(f.addr == c.addr && f.depth == c.depth && f.state == c.state && f.source == c.source,
              "returned Early-Z continuation fields changed");
      require(f.vertex_rgb == c.vertex_rgb && f.vertex_alpha == c.vertex_alpha &&
                  f.effect_tag == c.effect_tag && f.stencil_ref == c.stencil_ref,
              "returned continuation tail fields changed");
      require(f.texture_rgb == c.result_rgb && f.texture_alpha == c.result_alpha &&
                  f.texture_index == c.result_index && f.status == c.result_status,
              "returned complete result48 changed");
      fragment_beats.push_back(f);

      zref::FragmentPipeline::Frag oracle;
      oracle.addr = f.addr;
      oracle.depth = f.depth;
      oracle.state = f.state;
      oracle.vr = static_cast<uint8_t>(f.vertex_rgb >> 16);
      oracle.vg = static_cast<uint8_t>(f.vertex_rgb >> 8);
      oracle.vb = static_cast<uint8_t>(f.vertex_rgb);
      oracle.va = f.vertex_alpha;
      oracle.tag = f.effect_tag;
      oracle.sten_ref = f.stencil_ref;
      oracle.tr = static_cast<uint8_t>(f.texture_rgb >> 16);
      oracle.tg = static_cast<uint8_t>(f.texture_rgb >> 8);
      oracle.tb = static_cast<uint8_t>(f.texture_rgb);
      oracle.ta = f.texture_alpha;
      oracle.tidx = f.texture_index;
      const zref::FragmentPipeline::Out out =
          zref::FragmentPipeline::apply(oracle, oracle_tile[f.addr]);
      if (out.write) {
        oracle_tile[f.addr] = out.word;
        expected_writes.emplace_back(f.addr, out.word);
      }
      owner_score.pop_front();
      ++fragment_accepts;
    }

    if (e.drop_fire) {
      require(!owner_score.empty(), "sequence drop had no admitted owner");
      const OwnerExpected owner = owner_score.front();
      const Candidate& c = owner.candidate;
      const FragBeat f = observed_fragment();
      require(f.addr == c.addr && f.depth == c.depth && f.state == c.state &&
                  f.source == c.source && f.vertex_rgb == c.vertex_rgb &&
                  f.vertex_alpha == c.vertex_alpha && f.effect_tag == c.effect_tag &&
                  f.stencil_ref == c.stencil_ref,
              "dropped output corrupted one of 128 continuation bits");
      require(f.texture_rgb == c.result_rgb && f.texture_alpha == c.result_alpha &&
                  f.texture_index == c.result_index && f.status == c.result_status,
              "dropped output corrupted one of 48 result bits");
      if (e.mismatch) {
        require(!identity_drop_seen && owner.sequence == 1 &&
                    dut.returned_sequence_o == (owner.sequence ^ 1u),
                "first identity-only drop was not exactly sequence-1 corruption");
        identity_drop_seen = true;
      } else {
        require(dut.returned_sequence_o == owner.sequence,
                "later abort drop changed its original returned sequence");
      }
      owner_score.pop_front();
      ++drops;
    }

    if (e.tile_wr_fire) {
      require(!expected_writes.empty(), "real fragment wrote when zref predicted no write");
      const auto expected = expected_writes.front();
      expected_writes.pop_front();
      require(dut.tile_wr_addr_o == expected.first && dut.tile_wr_data_o == expected.second,
              "real fragment write differed from zref::FragmentPipeline");
      tile[dut.tile_wr_addr_o] = dut.tile_wr_data_o;
      ++physical_writes;
    }
    if (e.tile_rd_fire) accepted_tile_reads.push_back(dut.tile_rd_addr_o);

    const uint32_t accepted_fill_line = dut.fill_req_addr_o;
    const uint8_t accepted_rd_addr = dut.tile_rd_addr_o;

    dut.clk = 1;
    dut.eval();
    ++g_cycle;
    context.timeInc(1);

    if (fill_active) {
      if (fill_delay != 0) {
        --fill_delay;
      } else {
        ++fill_beat;
        if (fill_beat == 8) {
          fill_active = false;
          fill_beat = 0;
        }
      }
    }
    if (e.fill_req_fire) {
      require(!fill_active, "texture cache issued overlapping fills");
      fill_active = true;
      fill_line = accepted_fill_line;
      fill_beat = 0;
      fill_delay = next_fill_delay;
      next_fill_delay = 0;
    }

    // Fixed one-cycle tile response.  Writes are applied above before the new
    // response data is selected, matching the store's write-first collision law.
    tile_rsp_valid = e.tile_rd_fire;
    if (e.tile_rd_fire) tile_rsp_data = tile[accepted_rd_addr];
    // The external RAM's registered response changes immediately after this
    // clock edge.  Re-evaluate combinational consumers at clk=1 so the model
    // never presents a false between-edge protocol error.
    dut.tile_rd_valid_i = tile_rsp_valid;
    dut.tile_rd_data_i = tile_rsp_data;
    dut.eval();
    return e;
  }

  void reset() {
    dut.rst_n = 0;
    dut.skid_rst_n_i = 0;
    for (unsigned n = 0; n < 4; ++n) step();
    dut.rst_n = 1;
    dut.skid_rst_n_i = 1;
    for (unsigned n = 0; n < 4; ++n) step();
    require(dut.texture_quiet_o && dut.fragment_idle_o, "Packet-C harness did not reset quiet");
  }

  void replace_last_score(const Candidate& candidate) {
    require(!skid_score.empty(), "accepted input was not recorded in skid scoreboard");
    skid_score.back() = candidate;
  }

  void offer(const Candidate& candidate) {
    drive_wide(dut.in_data_i, candidate.packed);
    dut.in_valid_i = 1;
    for (unsigned n = 0; n < 20000; ++n) {
      const Events e = step();
      if (e.in_fire) {
        replace_last_score(candidate);
        dut.in_valid_i = 0;
        return;
      }
    }
    fail("timeout offering candidate to skid");
  }

  void wait_stage_accepts(uint32_t target, unsigned limit = 20000) {
    for (unsigned n = 0; n < limit && stage_accepts < target; ++n) step();
    require(stage_accepts == target, "timeout waiting for stage admissions");
  }

  void wait_fragment_accepts(uint32_t target, unsigned limit = 20000) {
    for (unsigned n = 0; n < limit && fragment_accepts < target; ++n) step();
    require(fragment_accepts == target, "timeout waiting for fragment admissions");
  }

  void wait_drained(unsigned limit = 20000) {
    for (unsigned n = 0; n < limit; ++n) {
      dut.clk = 0;
      dut.eval();
      if (dut.texture_quiet_o && dut.fragment_idle_o && expected_writes.empty()) return;
      step();
    }
    fail("timeout waiting for texture and fragment drain");
  }
};

void pulse_palette(Harness& h, uint8_t op, uint8_t index, uint16_t rgb565) {
  h.dut.pal_load_valid_i = 1;
  h.dut.pal_load_op_i = op;
  h.dut.pal_load_slot_i = 0;
  h.dut.pal_load_gen_i = 1;
  h.dut.pal_load_idx_i = index;
  h.dut.pal_load_rgb565_i = rgb565;
  h.dut.pal_load_crc_ok_i = 1;
  for (unsigned n = 0; n < 2000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    if (h.dut.pal_load_ready_o) {
      h.step();
      h.dut.pal_load_valid_i = 0;
      return;
    }
    h.step();
  }
  fail("timeout programming palette");
}

void program_palette(Harness& h) {
  pulse_palette(h, 0, 0, 0);
  for (unsigned index = 0; index < 256; ++index) {
    uint16_t colour = 0;
    if (index == 5) colour = 0x07e0u;  // green: bytes 00/ff/00, never raw index 5
    pulse_palette(h, 1, static_cast<uint8_t>(index), colour);
  }
  pulse_palette(h, 2, 0, 0);
}

uint8_t binding_command(Harness& h, uint8_t op, uint8_t generation, uint8_t selector,
                        const BindingRow& row, uint32_t crc) {
  h.dut.cfg_valid_i = 1;
  h.dut.cfg_op_i = op;
  h.dut.cfg_page_generation_i = generation;
  h.dut.cfg_selector_i = selector;
  h.dut.cfg_row_i[0] = row.base;
  h.dut.cfg_row_i[1] = row.mode;
  h.dut.cfg_row_i[2] = static_cast<uint32_t>(row.palette_slot & 3u) |
                       (static_cast<uint32_t>(row.palette_generation) << 2) |
                       (static_cast<uint32_t>(row.valid ? 1u : 0u) << 10);
  h.dut.cfg_crc32_i = crc;

  bool accepted = false;
  for (unsigned n = 0; n < 20000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    const bool fire = h.dut.cfg_valid_i && h.dut.cfg_ready_o;
    h.step();
    if (fire && !accepted) {
      accepted = true;
      h.dut.cfg_valid_i = 0;
    }
    h.dut.clk = 0;
    h.dut.eval();
    if (h.dut.cfg_rsp_valid_o) {
      const uint8_t status = h.dut.cfg_rsp_status_o;
      h.step();
      return status;
    }
  }
  fail("timeout waiting for binding command response");
}

void program_bindings(Harness& h) {
  std::array<BindingRow, 256> rows{};
  std::array<bool, 256> present{};
  rows[1] = BindingRow{0x00002000u, 0, 0, 1, true};
  rows[2] = BindingRow{0x00003000u, 0, 0, 1, true};
  present[1] = true;
  present[2] = true;
  const uint32_t crc = binding_crc(1, rows, present);
  const BindingRow zero{};
  require(binding_command(h, 0, 1, 0, zero, 0) == 0, "binding BEGIN failed");
  require(binding_command(h, 1, 1, 1, rows[1], 0) == 0, "binding row 1 failed");
  require(binding_command(h, 1, 1, 2, rows[2], 0) == 0, "binding row 2 failed");
  require(binding_command(h, 2, 1, 0, zero, crc) == 0, "binding END failed");
  require(h.dut.active_page_generation_o == 1, "binding generation did not activate");
}

void clear_recoverable(Harness& h) {
  h.wait_drained();
  h.dut.frame_fault_clear_valid_i = 1;
  for (unsigned n = 0; n < 2000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    const bool fire = h.dut.frame_fault_clear_ready_o;
    h.step();
    if (fire) {
      h.dut.frame_fault_clear_valid_i = 0;
      h.step();
      require(!h.dut.frame_fault_o, "accepted quiet clear left recoverable fault set");
      return;
    }
  }
  fail("timeout waiting for recoverable clear acceptance");
}

void test_skid_ab_hold(Harness& h) {
  begin_test("490-bit skid A/B hold and cancellation");
  h.score_inputs = false;
  h.dut.stage_admit_enable_i = 0;
  Wide490 a{};
  Wide490 b{};
  for (unsigned word = 0; word < a.size(); ++word) {
    a[word] = 0x13579bdfu ^ (0x10203041u * word);
    b[word] = 0xfedcba98u ^ (0x01020408u * word);
  }
  a[15] &= 0x3ffu;
  b[15] &= 0x3ffu;

  drive_wide(h.dut.in_data_i, a);
  h.dut.in_valid_i = 1;
  while (!h.step().in_fire) {
  }
  h.dut.in_valid_i = 0;
  h.step();
  require(h.dut.obs_cand_valid_o && wide_equal_port(a, h.dut.obs_cand_data_o),
          "skid head A was not the complete 490-bit packet");

  drive_wide(h.dut.in_data_i, b);
  h.dut.in_valid_i = 1;
  while (!h.step().in_fire) {
  }
  h.dut.in_valid_i = 0;
  require(h.dut.skid_level_o == 2, "A/B hold did not occupy both skid slots");
  for (unsigned n = 0; n < 8; ++n) {
    h.step();
    require(h.dut.obs_cand_valid_o && wide_equal_port(a, h.dut.obs_cand_data_o),
            "held skid A changed while later B occupied the second slot");
  }

  const uint32_t cancel_base = h.skid_cancel_events;
  h.dut.skid_cancel_i = 1;
  const auto cancel = h.step();
  require(cancel.skid_cancelled && h.dut.skid_level_o == 2,
          "test-only occupied-skid cancellation control did not fire before reset");
  h.dut.skid_cancel_i = 0;
  h.dut.skid_rst_n_i = 0;
  h.step();
  require(h.skid_cancel_events == cancel_base + 1,
          "occupied-skid cancellation event did not fire exactly once");
  h.dut.skid_rst_n_i = 1;
  require(h.dut.skid_level_o == 0, "test-only skid cancellation did not empty skid");
  h.dut.stage_admit_enable_i = 1;
  h.score_inputs = true;
}

void test_normal_hold_bubble_and_refusal(Harness& h) {
  begin_test("atomic admission, held fragment A/B, no bubble, terminal refusal");
  const uint32_t accept_base = h.stage_accepts;
  const uint32_t fragment_base = h.fragment_accepts;
  const std::size_t cycle_base = h.cand_fire_cycles.size();
  h.dut.fragment_pause_i = 1;

  for (uint8_t index = 0; index < 4; ++index)
    h.offer(make_count0(static_cast<uint8_t>(0x20u + index)));
  h.wait_stage_accepts(accept_base + 4);
  for (unsigned n = 1; n < 4; ++n)
    require(h.cand_fire_cycles[cycle_base + n] == h.cand_fire_cycles[cycle_base + n - 1] + 1,
            "atomic V3 admission inserted a bubble in an all-ready stream");

  for (unsigned n = 0; n < 200 && !h.dut.obs_frag_valid_o; ++n) h.step();
  require(h.dut.obs_frag_valid_o, "prepared ordered result never reached fragment boundary");
  const FragBeat held = h.observed_fragment();
  for (unsigned n = 0; n < 12; ++n) {
    h.step();
    require(h.dut.obs_frag_valid_o && h.observed_fragment() == held,
            "held fragment A changed while later owner B completed");
  }

  h.dut.fragment_pause_i = 0;
  for (unsigned n = 0; n < 2000; ++n) {
    const auto first = h.step();
    if (!first.fragment_fire) continue;
    require(h.step().fragment_fire, "prepared ordered fragment stream bubbled after first beat");
    require(h.step().fragment_fire, "prepared ordered fragment stream bubbled after second beat");
    require(h.step().fragment_fire, "prepared ordered fragment stream bubbled after third beat");
    break;
  }
  h.wait_fragment_accepts(fragment_base + 4);
  h.wait_drained();
  require(h.dut.texture_fragments_o == accept_base + 4,
          "fully drained V3 expansion census differed from stage admissions");
  require(h.stage_accepts == h.fragment_accepts + h.drops,
          "clean owner census violated S = F + sequence_drops");
  require(h.drops == 0 && h.dut.sequence_drop_count_o == 0,
          "clean stream changed sequence-drop accounting");

  // Recipe 1 requires count 2.  Count 1 must retire as a loud, status-bearing
  // terminal refusal through the ordinary ordered owner path.
  const Candidate refused = make_candidate(0x2f, 0, 0x7f2f, 0x2468acu, 0x9d, 0xd3, 0x6e, 1, 1, 1,
                                           0x112233u, 0x44, 0xff00ffu, 0xff, 0, 1);
  h.dut.fragment_pause_i = 1;
  h.offer(refused);
  h.wait_stage_accepts(accept_base + 5);
  for (unsigned n = 0; n < 2000 && !h.dut.obs_frag_valid_o; ++n) h.step();
  require(h.dut.obs_frag_valid_o, "terminal refusal did not reach fragment boundary");
  const FragBeat held_refusal = h.observed_fragment();
  require(held_refusal.status == 1 && held_refusal.texture_rgb == 0xff00ffu &&
              held_refusal.texture_alpha == 0xff && held_refusal.texture_index == 0,
          "terminal refusal did not carry full held result/status");
  for (unsigned n = 0; n < 8; ++n) {
    h.step();
    require(h.observed_fragment() == held_refusal,
            "terminal refusal status/result changed under fragment backpressure");
  }
  h.dut.fragment_pause_i = 0;
  h.wait_fragment_accepts(fragment_base + 5);
  h.wait_drained();
  require(h.dut.frame_fault_o && h.dut.combine_refused_o != 0,
          "illegal recipe/count did not fire recoverable refusal detector");
  clear_recoverable(h);
}

uint64_t tile_word(uint8_t r, uint8_t g, uint8_t b, uint8_t tag, uint32_t depth, uint8_t stencil);

void test_external_tile_semantics(Harness& h) {
  begin_test("external tile one-cycle, write-stall reissue, same-address hazard");

  // Fill all four fragment stages, then refuse the oldest write.  Once stage 1
  // is held, the real fragment must re-issue the same accepted read every cycle;
  // the external model returns each accepted read exactly one cycle later.
  h.dut.tile_wr_ready_i = 0;
  const std::size_t read_base = h.accepted_tile_reads.size();
  const uint32_t accept_base = h.fragment_accepts;
  for (uint8_t i = 0; i < 4; ++i) h.offer(make_count0(static_cast<uint8_t>(0x30u + i)));
  for (unsigned n = 0; n < 4000 && !h.dut.tile_wr_valid_o; ++n) h.step();
  require(h.dut.tile_wr_valid_o, "write-stall control never reached held write");
  const uint8_t held_addr = h.dut.tile_wr_addr_o;
  const uint64_t held_data = h.dut.tile_wr_data_o;
  for (unsigned n = 0; n < 8; ++n) {
    h.step();
    require(h.dut.tile_wr_valid_o && h.dut.tile_wr_addr_o == held_addr &&
                h.dut.tile_wr_data_o == held_data,
            "real fragment write packet changed while tile write stalled");
    require(!h.dut.fragment_error_o,
            "external accepted-read model missed its exact one-cycle response");
  }
  bool saw_reissue = false;
  for (std::size_t i = read_base + 1; i < h.accepted_tile_reads.size(); ++i)
    if (h.accepted_tile_reads[i] == h.accepted_tile_reads[i - 1]) saw_reissue = true;
  require(saw_reissue, "write stall did not re-issue the held stage-1 read");
  h.dut.tile_wr_ready_i = 1;
  h.wait_fragment_accepts(accept_base + 4);
  h.wait_drained();
  require(!h.dut.fragment_error_o && h.expected_writes.empty(),
          "write-stall drain violated tile response/write accounting");

  // Two back-to-back fragments at one address exercise the real fragment's
  // explicit in-flight hazard fence.  The external model still applies writes
  // before selecting any same-cycle accepted read response, but this composed
  // control correctly expects the fence—not an unreachable collision event.
  const uint8_t addr = 0x38;
  h.tile[addr] = h.oracle_tile[addr] = tile_word(10, 20, 30, 0x11, 0, 0x22);
  const uint32_t halo = zref::FragmentPipeline::star_halo_additive().pack();
  const uint32_t before = h.fragment_accepts;
  h.offer(make_candidate(addr, halo, 0x8838, 0x050607u, 0xff, 0x91, 0x31, 0, 0, 0, 0x010203u, 0xff,
                         0x010203u, 0xff, 0, 0));
  h.offer(make_candidate(addr, halo, 0x9838, 0x111213u, 0xff, 0x92, 0x32, 0, 0, 0, 0x040506u, 0xff,
                         0x040506u, 0xff, 0, 0));
  h.wait_fragment_accepts(before + 2);
  h.wait_drained();
  require(h.tile[addr] == h.oracle_tile[addr],
          "same-address hazard/drain result differed from zref::FragmentPipeline");
}

uint64_t tile_word(uint8_t r, uint8_t g, uint8_t b, uint8_t tag, uint32_t depth, uint8_t stencil) {
  zref::TileStore::Word w;
  w.r = r;
  w.g = g;
  w.b = b;
  w.tag = tag;
  w.depth = depth;
  w.stencil = stencil;
  return w.pack();
}

void test_star_fragment_differential(Harness& h) {
  begin_test("real CLUT8/palette star disc and halo through zref fragment");
  const uint8_t disc_zero_addr = 0x40;
  const uint8_t disc_five_addr = 0x41;
  const uint8_t halo_zero_addr = 0x42;
  h.tile[disc_zero_addr] = h.oracle_tile[disc_zero_addr] =
      tile_word(0x19, 0x2a, 0x3b, 0x17, 0, 0x55);
  h.tile[disc_five_addr] = h.oracle_tile[disc_five_addr] =
      tile_word(0x09, 0x0a, 0x0b, 0x18, 0, 0x56);
  h.tile[halo_zero_addr] = h.oracle_tile[halo_zero_addr] =
      tile_word(0x21, 0x32, 0x43, 0x19, 0, 0x57);
  const uint64_t disc_zero_before = h.tile[disc_zero_addr];
  const uint64_t halo_zero_before = h.tile[halo_zero_addr];
  const std::size_t beat_base = h.fragment_beats.size();
  const uint32_t write_base = h.physical_writes;

  const uint32_t disc_state = zref::FragmentPipeline::star_disc_masked().pack();
  const uint32_t halo_state = zref::FragmentPipeline::star_halo_additive().pack();
  h.offer(make_candidate(disc_zero_addr, disc_state, 0x9140, 0x314159u, 0xff, 0xe1, 0x61, 1, 2, 0,
                         0, 0, 0x000000u, 0xff, 0, 0));
  h.offer(make_candidate(disc_five_addr, disc_state, 0x9141, 0x123456u, 0xff, 0xe2, 0x62, 1, 1, 0,
                         0, 0, 0x00ff00u, 0xff, 5, 0));
  h.offer(make_candidate(halo_zero_addr, halo_state, 0x9142, 0x000000u, 0xff, 0xe3, 0x63, 1, 2, 0,
                         0, 0, 0x000000u, 0xff, 0, 0));
  h.wait_fragment_accepts(h.fragment_accepts + 3);
  h.wait_drained();

  require(h.fragment_beats.size() == beat_base + 3,
          "star sequence did not expose exactly three complete fragment beats");
  const FragBeat& zero = h.fragment_beats[beat_base + 0];
  const FragBeat& five = h.fragment_beats[beat_base + 1];
  require(zero.texture_index == 0 && zero.texture_alpha == 0xff,
          "CLUT index zero/opaque-alpha control did not reach real fragment");
  require(h.tile[disc_zero_addr] == disc_zero_before,
          "raw CLUT index zero failed to kill star disc despite opaque alpha");
  require(five.texture_index == 5 && five.texture_rgb == 0x00ff00u && five.texture_index != 0x00 &&
              five.texture_index != 0xff,
          "raw index was lost or reconstructed from a palette colour byte");
  require(zref::TileStore::Word::unpack(h.tile[disc_five_addr]).tag == 0x45,
          "nonzero star disc tag was not {01,index[5:0]}");
  const auto halo_before = zref::TileStore::Word::unpack(halo_zero_before);
  const auto halo_after = zref::TileStore::Word::unpack(h.tile[halo_zero_addr]);
  require(halo_after.r == halo_before.r && halo_after.g == halo_before.g &&
              halo_after.b == halo_before.b,
          "palette-zero halo was not an additive RGB identity");
  require(h.physical_writes == write_base + 2,
          "star disc/halo physical write count differed from zref oracle");
  require(h.expected_writes.empty() && !h.dut.fragment_error_o,
          "real fragment/tile model ended with unmatched write or protocol fault");
}

void test_mismatch_terminal(Harness& h, bool expect_old_ready) {
  begin_test(expect_old_ready ? "old-ready mismatch strands ordered head"
                              : "identity mismatch always-drain, release, clear rebase");
  program_palette(h);
  program_bindings(h);

  // Sequence 0 is accepted by the real fragment before the delayed sequence-1
  // sample arrives, but its physical write is deliberately held.  Packet C may
  // not retract this earlier acceptance; the one older write is allowed after
  // the sequence alarm, while the mismatch and all later V3 beats are suppressed.
  h.dut.tile_wr_ready_i = 0;
  h.offer(make_count0(0x60));
  h.wait_stage_accepts(1);
  h.wait_fragment_accepts(1);
  for (unsigned n = 0; n < 2000 && !h.dut.tile_wr_valid_o; ++n) h.step();
  require(h.dut.tile_wr_valid_o && !h.dut.fragment_idle_o,
          "sequence-0 control did not hold an earlier accepted fragment write");
  require(h.dut.expected_sequence_o == 1 && h.dut.admission_sequence_o == 1,
          "sequence-0 control did not retire into the real fragment before setup");
  const uint32_t writes_before_mismatch = h.physical_writes;

  h.next_fill_delay = 80;
  h.offer(make_candidate(0x61, 0, 0xa161, 0x456789u, 0xd1, 0xb1, 0x71, 1, 1, 0, 0, 0, 0x00ff00u,
                         0xff, 5, 0));
  h.offer(make_count0(0x62));
  h.offer(make_count0(0x63));
  h.wait_stage_accepts(4);
  for (unsigned n = 0; n < 20000 && !h.fill_active; ++n) {
    require(h.stage_accepts == 4, "downstream fill wait changed the exact stage-admission census");
    h.step();
  }
  require(h.fill_active && h.fill_delay != 0,
          "delayed sequence-1 fill request was not explicitly accepted");

  // Hold one extra candidate in the upstream skid.  It is enabled on the exact
  // mismatch cycle; Packet C must suppress same-edge owner admission, and the
  // harness cancels it only after synthetic RELEASE.
  h.dut.stage_admit_enable_i = 0;
  h.offer(make_count0(0x64));
  require(h.dut.skid_level_o != 0, "mismatch control did not retain upstream skid work");
  const uint32_t admission_before = h.dut.admission_sequence_o;
  h.fill_delay = 0;

  bool saw_mismatch = false;
  for (unsigned n = 0; n < 20000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    if (h.dut.sequence_mismatch_o) {
      h.dut.stage_admit_enable_i = 1;
      h.dut.frame_fault_clear_valid_i = 1;
      h.dut.eval();
      require(!h.dut.cand_fire_o && !h.dut.obs_cand_ready_o,
              "mismatch failed to suppress same-edge candidate admission");
      require(!h.dut.frame_fault_clear_ready_o,
              "mismatch-edge clear request was incorrectly accepted");
      const auto event = h.step();
      h.dut.frame_fault_clear_valid_i = 0;
      require(event.mismatch, "sequence mismatch pulse vanished before its edge");
      if (!expect_old_ready)
        require(event.drop_fire, "mismatched V3 head was not accepted as a drop");
      saw_mismatch = true;
      break;
    }
    h.step();
  }
  require(saw_mismatch, "identity corruption did not fire sequence detector");
  require(h.dut.admission_sequence_o == admission_before,
          "same-edge mismatch changed admission sequence");
  require(h.dut.sequence_abort_o && h.dut.frame_fault_o,
          "sequence mismatch did not latch abort/frame fault");
  h.dut.tile_wr_ready_i = 1;  // permit the sole pre-alarm accepted fragment write

  if (expect_old_ready) {
    for (unsigned n = 0; n < 512; ++n) h.step();
    require(h.dut.sequence_drop_count_o == 0 && h.drops == 0,
            "old-ready mutant unexpectedly drained its mismatched head");
    require(!h.dut.texture_quiet_o && !h.dut.frame_fault_clear_ready_o,
            "old-ready mutant falsely reached quiet/clear-ready");
    h.dut.classify_i = 1;
    h.dut.clk = 0;
    h.dut.eval();
    require(!h.dut.synthetic_release_o && !h.dut.synthetic_publish_o,
            "stranded old-ready head falsely classified a terminal event");
    require(h.dut.fragment_idle_o && h.physical_writes == writes_before_mismatch + 1,
            "old-ready control did not allow exactly the earlier accepted write");
    std::printf("packet-c old-ready deadlock mutant FIRED\n");
    return;
  }

  for (unsigned n = 0; n < 20000; ++n) {
    h.dut.clk = 0;
    h.dut.eval();
    if (h.dut.texture_quiet_o && h.dut.fragment_idle_o && h.expected_writes.empty()) break;
    h.step();
  }
  require(h.dut.texture_quiet_o && h.dut.fragment_idle_o && h.expected_writes.empty(),
          "identity abort did not reach finite texture+fragment drain");
  require(h.stage_accepts == 4 && h.fragment_accepts == 1 && h.drops == 3 &&
              h.dut.sequence_drop_count_o == 3,
          "mismatch accounting did not satisfy S=F+SD with exact later drops");
  require(h.owner_score.empty() && h.dut.texture_fragments_o == 4 && h.identity_drop_seen,
          "always-drain did not release every owner with fired identity control");
  require(h.physical_writes == writes_before_mismatch + 1,
          "mismatch path did not allow exactly the pre-alarm accepted write");

  h.dut.classify_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
  require(h.dut.synthetic_release_o && !h.dut.synthetic_publish_o,
          "faulted finite drain was not RELEASE/no-PUBLISH");
  const auto classification = h.step();
  require(classification.release && !classification.publish,
          "synthetic RELEASE classification did not survive its handshake edge");
  h.dut.classify_i = 0;

  // Cancel the occupied upstream skid only after synthetic RELEASE, observe the
  // cancellation request while occupancy is still live, and only then assert
  // its private reset.  No clear request is active during either operation.
  const uint32_t cancel_base = h.skid_cancel_events;
  require(!h.dut.frame_fault_clear_valid_i && h.dut.skid_level_o != 0,
          "post-RELEASE cancellation lacked held skid work or had an early clear");
  h.dut.skid_cancel_i = 1;
  const auto cancel = h.step();
  require(cancel.skid_cancelled && h.dut.skid_level_o != 0,
          "post-RELEASE skid cancellation did not observe pre-reset occupancy");
  h.dut.skid_cancel_i = 0;
  h.dut.skid_rst_n_i = 0;
  h.step();
  require(h.skid_cancel_events == cancel_base + 1 && h.dut.skid_level_o == 0,
          "post-RELEASE cancellation did not fire once then empty the skid");
  h.skid_score.clear();
  h.dut.skid_rst_n_i = 1;

  // Only now issue a new clear request.  Its accepted quiet handshake rebases
  // expected_sequence to the current next-admission sequence; the drop counter
  // is lifetime-monotonic and must not participate in frame clear.
  h.dut.frame_fault_clear_valid_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
  require(h.dut.frame_fault_clear_ready_o,
          "post-drain/post-RELEASE/post-cancel clear was not ready");
  h.step();
  h.dut.frame_fault_clear_valid_i = 0;
  h.step();
  require(!h.dut.sequence_abort_o && !h.dut.frame_fault_o &&
              h.dut.expected_sequence_o == h.dut.admission_sequence_o &&
              h.dut.admission_sequence_o == 4,
          "accepted quiet clear did not clear/rebase sequence state");
  require(h.dut.sequence_drop_count_o == 3,
          "accepted frame clear reset the monotonic sequence-drop counter");

  const uint32_t fragment_before = h.fragment_accepts;
  h.offer(make_count0(0x65));
  h.wait_stage_accepts(5);
  h.wait_fragment_accepts(fragment_before + 1);
  h.wait_drained();
  require(h.dut.expected_sequence_o == 5 && h.dut.admission_sequence_o == 5,
          "clean post-clear sequence 4 did not retire and advance both cursors to 5");
  require(h.dut.sequence_drop_count_o == 3 && !h.dut.frame_fault_o,
          "clean next frame changed drop count or reasserted fault");
  h.dut.classify_i = 1;
  h.dut.clk = 0;
  h.dut.eval();
  require(!h.dut.synthetic_release_o && h.dut.synthetic_publish_o,
          "clean post-clear frame was not synthetic PUBLISH/no-RELEASE");
  h.dut.classify_i = 0;
  std::printf("packet-c identity-only mismatch detector FIRED\n");
}

void run_healthy() {
  Harness h;
  h.reset();
  require(!h.dut.shadow_present_o && h.dut.meta_shadow_mismatch_o == 0 &&
              h.dut.meta_shadow_reads_o == 0,
          "explicit MIGRATION_SHADOWS=0 retained laboratory shadow state");
  test_skid_ab_hold(h);
  program_palette(h);
  program_bindings(h);
  test_normal_hold_bubble_and_refusal(h);
  test_external_tile_semantics(h);
  test_star_fragment_differential(h);
  h.wait_drained();
  require(h.owner_score.empty() && h.skid_score.empty() && h.expected_writes.empty(),
          "healthy gate ended with unaccounted work");
  require(h.stage_accepts == h.fragment_accepts + h.drops,
          "healthy final owner census violated S=F+SD");
}

void run_mutant(bool old_ready) {
  Harness h;
  h.reset();
  require(!h.dut.shadow_present_o && h.dut.meta_shadow_mismatch_o == 0 &&
              h.dut.meta_shadow_reads_o == 0,
          "mutant closure did not retain explicit MIGRATION_SHADOWS=0 profile");
  test_mismatch_terminal(h, old_ready);
}

}  // namespace

double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#if defined(PACKET_C_EXPECT_OLD_READY)
  run_mutant(true);
#elif defined(PACKET_C_EXPECT_IDENTITY_ABORT)
  run_mutant(false);
#else
  run_healthy();
#endif
  std::printf("packet-c directed PASS tests=%u checks=%u cycles=%llu\n", g_tests, g_checks,
              static_cast<unsigned long long>(g_cycle));
  zhao::exit_hard(0);
}
