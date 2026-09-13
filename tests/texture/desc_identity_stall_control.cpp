// desc_identity_stall_control.cpp
//
// Independent full-descriptor identity scoreboard for the selected V3
// descriptor path: accepted owner-tagged PERSPUV records -> held descriptor ->
// expander sample handles {slot,generation,sample}. Every descriptor field is
// reconstructed from the accepted input, never from a DUT counter or sidecar.
//
// The same driver is also built against three separately renamed committed
// mutants. Their inverse-polarity lanes prove, independently, that descriptor
// swaps, valid withdrawal under stall, and a silently dropped AUX beat are all
// detected. Golden acceptance requires exact conservation and persistence.
#include "Vtb_desc_join_expand.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

namespace {

struct Desc {
  uint64_t ctx;
  uint8_t gen, lod, cls, aux, count, pslot, pgen, mosa, mosb, mosw, bsel;
};

struct Accepted {
  Desc desc;
  uint16_t owner;
  int32_t u, v;
  uint8_t sat, dz;
};

struct Req {
  uint32_t src;
  int32_t u, v;
  uint8_t lod, pslot, pgen;
};

struct FullBeat {
  uint16_t owner;
  int32_t u, v;
  uint64_t ctx;
  uint8_t sat, dz, binding, lod, count, aux, cls, pslot, pgen;
};

struct ReqBeat {
  uint32_t src;
  int32_t u, v;
  uint8_t lod, pslot, pgen;
};

struct AuxBeat {
  uint16_t owner;
  uint64_t ctx;
};

struct MosaicBeat {
  uint8_t a, b, weight;
};

Desc make_desc(unsigned slot) {
  Desc d{};
  d.ctx = (0xD35C0000ull + slot * 0x10203ull) << 32 |
          (0xA11C0000ull + slot * 0x40507ull);
  d.gen = static_cast<uint8_t>(0x31u + slot);
  d.lod = static_cast<uint8_t>(slot * 7u + 3u);
  d.cls = static_cast<uint8_t>(slot & 3u);
  d.aux = static_cast<uint8_t>(slot & 1u);
  d.count = static_cast<uint8_t>(1u + (slot & 1u));
  d.pslot = static_cast<uint8_t>((slot + 1u) & 3u);
  d.pgen = static_cast<uint8_t>(0x91u + slot * 3u);
  d.mosa = static_cast<uint8_t>(slot * 5u + 11u);
  d.mosb = static_cast<uint8_t>(slot * 9u + 17u);
  d.mosw = static_cast<uint8_t>(0xE7u - slot * 3u);
  d.bsel = static_cast<uint8_t>(slot ^ 0xA6u);
  return d;
}

uint8_t sane_class(uint8_t cls) { return cls == 3u ? 1u : cls; }
uint16_t owner_of(unsigned slot, uint8_t gen) {
  return static_cast<uint16_t>(((slot & 0x3Fu) << 8) | gen);
}

void tick(Vtb_desc_join_expand* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
  d->clk = 0;
  d->eval();
}

void clear_inputs(Vtb_desc_join_expand* d) {
  d->wr_valid_i = 0;
  d->p_valid_i = 0;
  d->p_u_i = 0;
  d->p_v_i = 0;
  d->p_tag_i = 0;
  d->p_sat_i = 0;
  d->p_dz_i = 0;
  d->req_ready_i = 0;
  d->aux_ready_i = 0;
  d->m_ready_i = 0;
}

void load(Vtb_desc_join_expand* d, unsigned slot, const Desc& r) {
  d->wr_valid_i = 1;
  d->wr_slot_i = static_cast<uint8_t>(slot);
  d->wr_owner_gen_i = r.gen;
  d->wr_aux_context_i = r.ctx;
  d->wr_lod_i = r.lod;
  d->wr_raw_class_i = r.cls;
  d->wr_needs_aux_i = r.aux;
  d->wr_sample_count_i = r.count;
  d->wr_palette_slot_i = r.pslot;
  d->wr_palette_gen_i = r.pgen;
  d->wr_mosaic_mat_a_i = r.mosa;
  d->wr_mosaic_mat_b_i = r.mosb;
  d->wr_mosaic_weight_i = r.mosw;
  d->wr_binding_sel_i = r.bsel;
  tick(d);
  d->wr_valid_i = 0;
}

FullBeat full_beat(Vtb_desc_join_expand* d) {
  return {static_cast<uint16_t>(d->f_owner_o), static_cast<int32_t>(d->f_u_o),
          static_cast<int32_t>(d->f_v_o), d->f_ctx_o,
          static_cast<uint8_t>(d->f_sat_o), static_cast<uint8_t>(d->f_depth_zero_o),
          static_cast<uint8_t>(d->f_binding_o), static_cast<uint8_t>(d->f_lod_o),
          static_cast<uint8_t>(d->f_count_o), static_cast<uint8_t>(d->f_aux_o),
          static_cast<uint8_t>(d->f_class_o), static_cast<uint8_t>(d->f_pal_slot_o),
          static_cast<uint8_t>(d->f_pal_gen_o)};
}

ReqBeat req_beat(Vtb_desc_join_expand* d) {
  return {d->req_src_id_o, static_cast<int32_t>(d->req_u_o),
          static_cast<int32_t>(d->req_v_o), static_cast<uint8_t>(d->req_lod_o),
          static_cast<uint8_t>(d->req_pal_slot_o),
          static_cast<uint8_t>(d->req_pal_gen_o)};
}

AuxBeat aux_beat(Vtb_desc_join_expand* d) {
  return {static_cast<uint16_t>(d->aux_owner_o), d->aux_ctx_o};
}

MosaicBeat mosaic_beat(Vtb_desc_join_expand* d) {
  return {static_cast<uint8_t>(d->m_mat_a_o),
          static_cast<uint8_t>(d->m_mat_b_o),
          static_cast<uint8_t>(d->m_weight_o)};
}

bool same(const FullBeat& a, const FullBeat& b) {
  return a.owner == b.owner && a.u == b.u && a.v == b.v && a.ctx == b.ctx &&
         a.sat == b.sat && a.dz == b.dz && a.binding == b.binding &&
         a.lod == b.lod && a.count == b.count && a.aux == b.aux &&
         a.cls == b.cls && a.pslot == b.pslot && a.pgen == b.pgen;
}
bool same(const ReqBeat& a, const ReqBeat& b) {
  return a.src == b.src && a.u == b.u && a.v == b.v && a.lod == b.lod &&
         a.pslot == b.pslot && a.pgen == b.pgen;
}
bool same(const AuxBeat& a, const AuxBeat& b) {
  return a.owner == b.owner && a.ctx == b.ctx;
}
bool same(const MosaicBeat& a, const MosaicBeat& b) {
  return a.a == b.a && a.b == b.b && a.weight == b.weight;
}

template <typename Beat>
struct HoldTracker {
  bool active = false;
  bool withdrawal_reported = false;
  Beat held{};
  int stalled_cycles = 0;
  int payload_errors = 0;
  int withdrawal_errors = 0;

  void observe(bool valid, bool ready, const Beat& now) {
    if (active) {
      if (!valid) {
        if (!withdrawal_reported) {
          ++withdrawal_errors;
          withdrawal_reported = true;
        }
        return;
      }
      if (!same(now, held)) ++payload_errors;
      if (ready) {
        active = false;
        withdrawal_reported = false;
      } else {
        ++stalled_cycles;
      }
      return;
    }
    if (valid && !ready) {
      active = true;
      withdrawal_reported = false;
      held = now;
      ++stalled_cycles;
    }
  }
};

FullBeat expected_full(const Accepted& a) {
  return {a.owner, a.u, a.v, a.desc.ctx, a.sat, a.dz, a.desc.bsel,
          a.desc.lod, a.desc.count, a.desc.aux, a.desc.cls,
          a.desc.pslot, a.desc.pgen};
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  auto* d = new Vtb_desc_join_expand;
  clear_inputs(d);
  d->rst_n = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  constexpr int kRows = 64;
  std::vector<Desc> rows;
  rows.reserve(kRows);
  for (int slot = 0; slot < kRows; ++slot) {
    rows.push_back(make_desc(static_cast<unsigned>(slot)));
    load(d, static_cast<unsigned>(slot), rows.back());
  }

  std::deque<Accepted> full_expected;
  std::deque<Req> req_expected;
  std::deque<AuxBeat> aux_expected;
  std::deque<MosaicBeat> mosaic_expected;

  int sent = 0, full_seen = 0, req_seen = 0, aux_seen = 0, mosaic_seen = 0;
  int expected_requests = 0, expected_aux = 0, expected_mosaic = 0;
  int identity_errors = 0, request_errors = 0, aux_errors = 0, mosaic_errors = 0;
  HoldTracker<FullBeat> full_hold;
  HoldTracker<ReqBeat> req_hold;
  HoldTracker<AuxBeat> aux_hold;
  HoldTracker<MosaicBeat> mosaic_hold;
  int quiet_cycles = 0;
  bool completed = false;
  uint32_t rng = 0x51A11EDu;

  for (int cycle = 0; cycle < 200000; ++cycle) {
    rng = rng * 1664525u + 1013904223u;
    const bool forced_open = cycle < 16;
    const bool forced_closed = cycle >= 24 && cycle < 88;
    d->req_ready_i = forced_open ||
                     (!forced_closed && (((rng >> 3) & 3u) != 0u));
    d->aux_ready_i = forced_open ||
                     (!forced_closed && (((rng >> 11) & 1u) != 0u));
    d->m_ready_i = forced_open ||
                   (!forced_closed && (((rng >> 19) & 7u) != 0u));

    if (sent < kRows) {
      const unsigned slot = static_cast<unsigned>((sent * 17) & 63);
      const Desc& r = rows[slot];
      d->p_valid_i = 1;
      d->p_tag_i = owner_of(slot, r.gen);
      d->p_u_i = static_cast<uint32_t>(0x11000000u + sent * 0x1021u);
      d->p_v_i = static_cast<uint32_t>(0x77000000u - sent * 0x203u);
      d->p_sat_i = static_cast<uint8_t>((sent >> 1) & 1);
      d->p_dz_i = static_cast<uint8_t>((sent >> 2) & 1);
    } else {
      d->p_valid_i = 0;
    }
    d->eval();

    const bool p_fire = d->p_valid_i && d->p_ready_o;
    const bool f_fire = d->f_valid_o && d->f_ready_o;
    const bool req_fire = d->req_valid_o && d->req_ready_i;
    const bool aux_fire = d->aux_valid_o && d->aux_ready_i;
    const bool mosaic_fire = d->m_valid_o && d->m_ready_i;

    full_hold.observe(d->f_valid_o, d->f_ready_o, full_beat(d));
    req_hold.observe(d->req_valid_o, d->req_ready_i, req_beat(d));
    aux_hold.observe(d->aux_valid_o, d->aux_ready_i, aux_beat(d));
    mosaic_hold.observe(d->m_valid_o, d->m_ready_i, mosaic_beat(d));

    if (p_fire) {
      const unsigned slot = static_cast<unsigned>((sent * 17) & 63);
      const Desc& r = rows[slot];
      Accepted accepted{r, owner_of(slot, r.gen), static_cast<int32_t>(d->p_u_i),
                        static_cast<int32_t>(d->p_v_i),
                        static_cast<uint8_t>(d->p_sat_i),
                        static_cast<uint8_t>(d->p_dz_i)};
      full_expected.push_back(accepted);
      for (unsigned sample = 0; sample < r.count; ++sample) {
        req_expected.push_back({
            (static_cast<uint32_t>(sane_class(r.cls)) << 16) |
                (slot << 10) | (sample << 8) | r.gen,
            accepted.u, accepted.v, r.lod, r.pslot, r.pgen});
      }
      expected_requests += r.count;
      if (r.aux) {
        aux_expected.push_back({accepted.owner, r.ctx});
        ++expected_aux;
      }
      mosaic_expected.push_back({r.mosa, r.mosb, r.mosw});
      ++expected_mosaic;
      ++sent;
    }

    if (f_fire) {
      ++full_seen;
      if (full_expected.empty()) {
        ++identity_errors;
      } else {
        if (!same(full_beat(d), expected_full(full_expected.front())))
          ++identity_errors;
        full_expected.pop_front();
      }
    }
    if (req_fire) {
      ++req_seen;
      if (req_expected.empty()) {
        ++request_errors;
      } else {
        const Req& e = req_expected.front();
        ReqBeat want{e.src, e.u, e.v, e.lod, e.pslot, e.pgen};
        if (!same(req_beat(d), want)) ++request_errors;
        req_expected.pop_front();
      }
    }
    if (aux_fire) {
      ++aux_seen;
      if (aux_expected.empty()) {
        ++aux_errors;
      } else {
        if (!same(aux_beat(d), aux_expected.front())) ++aux_errors;
        aux_expected.pop_front();
      }
    }
    if (mosaic_fire) {
      ++mosaic_seen;
      if (mosaic_expected.empty()) {
        ++mosaic_errors;
      } else {
        if (!same(mosaic_beat(d), mosaic_expected.front())) ++mosaic_errors;
        mosaic_expected.pop_front();
      }
    }

    tick(d);
    if (sent == kRows && !d->f_valid_o && !d->req_valid_o &&
        !d->aux_valid_o && !d->m_valid_o) {
      ++quiet_cycles;
      if (quiet_cycles == 64) {
        completed = true;
        break;
      }
    } else {
      quiet_cycles = 0;
    }
  }

  const int total_identity_errors =
      identity_errors + request_errors + aux_errors + mosaic_errors;
  const int payload_hold_errors =
      full_hold.payload_errors + req_hold.payload_errors +
      aux_hold.payload_errors + mosaic_hold.payload_errors;
  const int valid_withdrawal_errors =
      full_hold.withdrawal_errors + req_hold.withdrawal_errors +
      aux_hold.withdrawal_errors + mosaic_hold.withdrawal_errors;
  const int full_conservation_errors =
      static_cast<int>(full_expected.size()) + (full_seen != sent) +
      (full_hold.active ? 1 : 0);
  const int request_conservation_errors =
      static_cast<int>(req_expected.size()) +
      (req_seen != expected_requests) + (req_hold.active ? 1 : 0);
  const int aux_conservation_errors =
      static_cast<int>(aux_expected.size()) + (aux_seen != expected_aux) +
      (aux_hold.active ? 1 : 0);
  const int mosaic_conservation_errors =
      static_cast<int>(mosaic_expected.size()) +
      (mosaic_seen != expected_mosaic) + (mosaic_hold.active ? 1 : 0);

  std::printf(
      "  accepted %d | observed full %d/%d request %d/%d aux %d/%d "
      "mosaic %d/%d\n"
      "  independent mismatches full %d request %d aux %d mosaic %d\n"
      "  persistence payload %d withdrawal %d | remaining full %zu request %zu "
      "aux %zu mosaic %zu\n"
      "  stalled cycles full %d request %d aux %d mosaic %d | "
      "DUT gen mismatch %u | completed %d\n",
      sent, full_seen, sent, req_seen, expected_requests, aux_seen, expected_aux,
      mosaic_seen, expected_mosaic, identity_errors, request_errors, aux_errors,
      mosaic_errors, payload_hold_errors, valid_withdrawal_errors,
      full_expected.size(), req_expected.size(), aux_expected.size(),
      mosaic_expected.size(), full_hold.stalled_cycles, req_hold.stalled_cycles,
      aux_hold.stalled_cycles, mosaic_hold.stalled_cycles, d->gen_mismatch_o,
      completed ? 1 : 0);

  zhao::check(sent == kRows,
              "all owner-tagged descriptor inputs were accepted", kRows, sent);
  zhao::check(d->desc_reads_o == kRows && d->joined_o == kRows &&
                  d->exp_fragments_o == kRows &&
                  d->exp_requests_o == expected_requests &&
                  d->exp_overflow_o == 0,
              "internal read/join/fragment/request counters balance without overflow",
              expected_requests, d->exp_requests_o);
  zhao::check(d->gen_mismatch_o == 0,
              "the DUT generation counter remains independent of this scoreboard",
              0, d->gen_mismatch_o);
  zhao::check(completed,
              "the DUT reached 64 quiet cycles; no lane passed by timing out",
              1, completed ? 1 : 0);
  zhao::check(full_hold.stalled_cycles > 0 && req_hold.stalled_cycles > 0 &&
                  aux_hold.stalled_cycles > 0 &&
                  mosaic_hold.stalled_cycles > 0,
              "all four independently observed handshakes were genuinely stalled",
              1, (full_hold.stalled_cycles > 0 && req_hold.stalled_cycles > 0 &&
                  aux_hold.stalled_cycles > 0 &&
                  mosaic_hold.stalled_cycles > 0) ? 1 : 0);

#if defined(ZHAO_DESC_VALID_WITHDRAW_CONTROL)
  zhao::check(valid_withdrawal_errors > 0,
              "POSITIVE CONTROL: valid withdrawal after a stall is detected",
              1, valid_withdrawal_errors > 0 ? 1 : 0);
  zhao::check(full_conservation_errors == 0 && aux_conservation_errors == 0 &&
                  mosaic_conservation_errors == 0,
              "the withdrawal mutant is isolated to the request substream",
              0, full_conservation_errors + aux_conservation_errors +
                     mosaic_conservation_errors);
#elif defined(ZHAO_DESC_SUBSTREAM_DROP_CONTROL)
  zhao::check(valid_withdrawal_errors == 0 && payload_hold_errors == 0,
              "the drop control does not masquerade as a stall-persistence fault",
              0, valid_withdrawal_errors + payload_hold_errors);
  zhao::check(aux_conservation_errors > 0,
              "POSITIVE CONTROL: the missing AUX beat fires queue/count conservation",
              1, aux_conservation_errors > 0 ? 1 : 0);
  zhao::check(full_conservation_errors == 0 &&
                  request_conservation_errors == 0 &&
                  mosaic_conservation_errors == 0,
              "the committed drop mutant is isolated to the AUX substream",
              0, full_conservation_errors + request_conservation_errors +
                     mosaic_conservation_errors);
#elif defined(ZHAO_DESC_SLOTSWAP_CONTROL)
  zhao::check(valid_withdrawal_errors == 0 && payload_hold_errors == 0,
              "valid and complete payload persist through every stalled handshake",
              0, valid_withdrawal_errors + payload_hold_errors);
  zhao::check(full_conservation_errors == 0 &&
                  request_conservation_errors == 0 &&
                  aux_conservation_errors == 0 &&
                  mosaic_conservation_errors == 0,
              "all expected full/request/AUX/Mosaic queues drain at exact counts",
              0, full_conservation_errors + request_conservation_errors +
                     aux_conservation_errors + mosaic_conservation_errors);
  zhao::check(total_identity_errors > 0,
              "POSITIVE CONTROL: the independent identity scoreboard fires on "
              "the adjacent-slot mutant while aggregate counters balance",
              1, total_identity_errors > 0 ? 1 : 0);
#else
  zhao::check(valid_withdrawal_errors == 0 && payload_hold_errors == 0,
              "valid persists and the complete payload is stable through handshake",
              0, valid_withdrawal_errors + payload_hold_errors);
  zhao::check(full_conservation_errors == 0,
              "every expected full-descriptor beat drains exactly once",
              0, full_conservation_errors);
  zhao::check(request_conservation_errors == 0,
              "every expected {slot,generation,sample} request drains at exact count",
              0, request_conservation_errors);
  zhao::check(aux_conservation_errors == 0,
              "every expected AUX beat drains at exact count",
              0, aux_conservation_errors);
  zhao::check(mosaic_conservation_errors == 0,
              "every expected Mosaic beat drains at exact count",
              0, mosaic_conservation_errors);
  zhao::check(total_identity_errors == 0,
              "full descriptor identity and all downstream payloads remain exact",
              0, total_identity_errors);
#endif

  const int rc = zhao::report_and_exit("desc_identity_stall_control");
  delete d;
  zhao::exit_hard(rc);
}
