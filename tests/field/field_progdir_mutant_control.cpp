// field_progdir_mutant_control.cpp -- THE POSITIVE CONTROL for the FIELD program
// directory differential, driving the committed MUTANT.
//
// field_progdir_differential asserts transaction identity against the retained
// oracle, and on the LRU-TIE law that claim can only be demonstrated after a
// stamp wrap. tests/mutants/zhao_field_progdir_scan_mutant.sv inverts the tie
// rule (`<` -> `<=`, highest index wins); THIS TEST PASSES WHEN THE
// DIFFERENTIAL FAILS on the tie script -- inverse polarity, evidence about the
// instrument, not about the design.
//
// The non-tie directed script is the other half: the mutant AGREES with the
// oracle on every one of those ops, every counter balancing. That is why a
// suite without a reachable wrap would wave the broken tie rule through, and
// why the ENTRIES=2 LRUW=2 elaboration exists.
//
// Built from tb_field_progdir_diff.sv with -DPROGDIR_MUTANT, -GENTRIES=2 -GLRUW=2.

#include <cstdio>
#include <vector>

#include "field_progdir_diff.hpp"

namespace {

using progdir::Harness;
using progdir::Op;

unsigned run_script(Harness& h, const std::vector<Op>& s, const char* tag) {
  unsigned mask = 0;
  unsigned k = 0;
  for (const Op& op : s) {
    char t[96];
    std::snprintf(t, sizeof t, "%s op %u", tag, k++);
    mask |= h.run(op, t);
  }
  return mask;
}

}  // namespace

int main() {
  progdir::Top top;
  Harness h(top);
  h.reset();

  std::printf("field_progdir_mutant_control: ENTRIES=%u LRUW=%u, candidate side = MUTANT\n",
              progdir::kEntries, progdir::kLruW);

  // ---- half 1: the weak vectors. The mutant must AGREE on every non-tie law.
  const unsigned weak = run_script(h, progdir::script_directed(), "weak");
  const int weak_failures = zhao::check_failures();
  std::printf("control: non-tie directed script -> mismatch mask 0x%x, %d check failures "
              "(expected 0: the mutant passes every weak vector)\n", weak, weak_failures);

  // ---- half 2: the tie. The mutant must DISAGREE, and only on the commit slot
  // first -- counters balance on the very op the victim is chosen.
  unsigned tie = 0;
  if (progdir::tie_reachable()) {
    tie = run_script(h, progdir::script_tie(), "TIE");
  } else {
    std::printf("control: tie not reachable at this elaboration -- build with ENTRIES=2 LRUW=2\n");
  }
  const int total_failures = zhao::check_failures();
  const progdir::Stats& st = h.stats();
  std::printf("control: tie script -> mismatch mask 0x%x; first mismatch at op %u mask 0x%x (%s)\n",
              tie, st.first_mismatch_op, st.first_mismatch_mask, st.first_mismatch_what.c_str());

  const bool weak_agreed = (weak == 0) && (weak_failures == 0);
  const bool tie_seen = (tie & progdir::kMisCmSlot) != 0;
  const bool slot_first = (st.first_mismatch_mask == progdir::kMisCmSlot);

  std::printf("\n=== field_progdir_mutant_control (INVERTED POLARITY) ===\n");
  std::printf("  mutant agrees on all %s non-tie ops : %s\n", "directed", weak_agreed ? "yes" : "NO");
  std::printf("  differential FAILS on the tie       : %s\n", tie_seen ? "yes" : "NO");
  std::printf("  first divergence is the victim SLOT alone, counters balanced: %s\n",
              slot_first ? "yes" : "NO");
  std::printf("  (%d raw check failures above are the checker firing on the mutant, as intended)\n",
              total_failures);
  const bool pass = weak_agreed && tie_seen && slot_first;
  std::printf("%s: field_progdir_mutant_control\n", pass ? "PASS" : "FAIL");
  top.final();
  zhao::exit_hard(pass ? 0 : 1);
}
