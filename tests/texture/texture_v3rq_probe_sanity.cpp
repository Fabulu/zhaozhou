// texture_v3rq_probe_sanity.cpp
//
// ---------------------------------------------------------------------------
// A FIT FIXTURE, CHECKED BEFORE IT IS TRUSTED
// ---------------------------------------------------------------------------
// `zhao_probe_v3rq_queue` exists to answer V3.1 §5.7's "local queue fit gate".
// It is a MEASUREMENT INSTRUMENT, and this repository's standing law is that the
// tool doing the measuring is itself a thing that has to be looked at.
//
// A registered-hash-sink wrapper fails silently in exactly one way: if the sink
// never changes, the fitter is free to delete the DUT, and the gate then reports
// a clean ALM/Fmax number for an empty design. Nothing downstream would notice.
//
// And it caught a second, different defect on its first run. The probe drove
// `wr_en_i` without gating on `full_o`, which the DUT forbids by assertion:
//
//     a_rq_no_write_when_full : assert (!(wr_en_i && full_o));
//
// That would NOT have shown up in the fit -- Quartus drops assertions -- so
// §5.7's gate would have characterised a production-shaped queue driven with
// illegal stimulus and reported a perfectly clean number for it.
#include "Vzhao_probe_v3rq_queue.h"

#include <cstdint>
#include <cstdio>
#include <set>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_probe_v3rq_queue* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

uint32_t lcg(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s;
}

}  // namespace

int main() {
  Vzhao_probe_v3rq_queue* d = new Vzhao_probe_v3rq_queue;

  d->rst_n = 0;
  d->stim_valid_i = 0;
  d->stim_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  std::set<uint32_t> seen;
  uint32_t rng = 0x1234567u;
  uint32_t last = d->hash_o;
  int changes = 0;
  const int kCycles = 4000;

  for (int i = 0; i < kCycles; ++i) {
    d->stim_valid_i = 1;
    d->stim_i = lcg(rng);
    d->eval();
    tick(d);
    d->eval();
    if (d->hash_o != last) ++changes;
    last = d->hash_o;
    seen.insert(d->hash_o);
  }

  std::printf("  hash changed on %d of %d cycles, %zu distinct values\n",
              changes, kCycles, seen.size());

  zhao::check(changes > 3000,
              "the probe's hash moves on essentially every cycle -- a fixture "
              "whose sink is constant lets the fitter delete the DUT and report "
              "a clean number for an empty design",
              1, changes > 3000 ? 1 : 0);
  zhao::check(seen.size() > 2000,
              "and it takes thousands of distinct values, so it is not a short "
              "cycle either",
              1, seen.size() > 2000 ? 1 : 0);

  const int rc = zhao::report_and_exit("texture_v3rq_probe_sanity");
  delete d;
  zhao::exit_hard(rc);
}
