// field_progdir_differential.cpp -- THE GATE for the scanned FIELD program
// directory: zhao_field_progdir (scanner + old-interface adapter) against the
// RETAINED oracle zhao_field_progcache, transaction by transaction, both
// elaborated side by side in tests/field/tb_field_progdir_diff.sv.
//
// Roadmap section 5 names the gate: "transaction-level differential against
// the retained ProgCache oracle, including LRU ties, wrap, invalid commit, held
// responses and interleaved old channels." Every one of those is an op kind or
// a directed case in field_progdir_diff.hpp. The engine is shared with the
// inverted-polarity control (field_progdir_mutant_control.cpp), which is what
// makes "this checker can see a tie fault" a demonstrated fact.
//
// Build at several elaborations (tests/CMakeLists.txt):
//   ENTRIES=16 LRUW=48   the shipping shape: directed, random, and the TIMING
//                        PROBE whose numbers the report quotes;
//   ENTRIES=2  LRUW=2    wrap every four stamps: the TIE case and random ties;
//   ENTRIES=3  LRUW=6    a non-power-of-two directory with wrap under random ops.
// -DTB_ENTRIES / -DTB_LRUW must match -GENTRIES / -GLRUW; case 0 checks that
// the compiled constants and the elaborated block agree before anything else.
//
// Usage: field_progdir_differential [--random N] [--no-probe]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "field_progdir_diff.hpp"

namespace {

using progdir::Harness;
using progdir::Op;
using progdir::kEntries;
using progdir::kLruW;

void run_script(Harness& h, const std::vector<Op>& s, const char* tag) {
  unsigned k = 0;
  for (const Op& op : s) {
    char t[96];
    std::snprintf(t, sizeof t, "%s op %u", tag, k++);
    h.run(op, t);
    if (zhao::check_failures() != 0) break;
  }
}

void print_intervals(const char* what, const std::vector<long>& iv) {
  long mn = -1, mx = -1;
  for (long v : iv) {
    if (mn < 0 || v < mn) mn = v;
    if (v > mx) mx = v;
  }
  std::printf("MEASURED %-52s accept-to-accept: min %ld  max %ld  (%zu intervals)\n", what, mn, mx, iv.size());
}

}  // namespace

int main(int argc, char** argv) {
  bool random_mode = false;
  unsigned iters = 0;
  bool probe = true;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_mode = true;
      iters = static_cast<unsigned>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--no-probe") == 0) {
      probe = false;
    }
  }

  progdir::Top top;
  Harness h(top);
  h.reset();

  std::printf("field_progdir_differential: ENTRIES=%u LRUW=%u (compiled constants)\n", kEntries, kLruW);

  // ---- 0. the compiled constants agree with the elaborated block -----------
  // Fill kEntries programs: none may evict; the next one MUST. If -D and -G
  // disagree this fails before any law is argued from it.
  {
    std::vector<Op> s;
    progdir::script_fill(s, kEntries);
    run_script(h, s, "elab");
    zhao::check(top.o_evictions_o == 0 && top.n_evictions_o == 0,
                "elab: filling ENTRIES programs evicts nothing on either side", 0,
                top.o_evictions_o + top.n_evictions_o);
    zhao::check(top.o_occupancy_o == kEntries && top.n_occupancy_o == kEntries,
                "elab: occupancy equals the compiled ENTRIES on both sides", kEntries,
                top.n_occupancy_o);
    std::vector<Op> s2;
    s2.push_back(progdir::commit(progdir::hash_of(kEntries + 77), true, "elab: one more evicts"));
    run_script(h, s2, "elab");
    zhao::check(top.o_evictions_o == 1 && top.n_evictions_o == 1,
                "elab: the (ENTRIES+1)th insert evicts on both sides", 1, top.n_evictions_o);
    h.run(progdir::reset_op(progdir::hash_of(0), "elab: reset"), "elab reset");
  }

  if (random_mode) {
    progdir::Prng rng(0x9D1Fu + kEntries * 131u + kLruW);
    for (unsigned it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      const unsigned n = 8 + rng.below(48 + 4 * kEntries);
      std::vector<Op> s = progdir::script_random(rng, n);
      char tag[48];
      std::snprintf(tag, sizeof tag, "random[%u]", it);
      run_script(h, s, tag);
    }
    std::printf("random: %u ops compared; candidate max latency lookup %ld commit %ld cycles\n",
                h.stats().ops, h.stats().max_lu_latency, h.stats().max_cm_latency);
    top.final();
    return zhao::report_and_exit("field_progdir_random");
  }

  // ---- 1. every non-wrapping law ---------------------------------------------
  run_script(h, progdir::script_directed(), "directed");

  // Every counter SEEN TO MOVE on the candidate, stated rather than implied:
  // agreement with the oracle would also hold for two directories that both
  // never counted anything.
  // (Maxima over op boundaries: the script resets the directory midway on
  // purpose, so the final counter values alone would read low.)
  {
    const progdir::Stats& st = h.stats();
    zhao::check(st.seen_hits > 0, "candidate hits_o fired", 1, st.seen_hits > 0);
    zhao::check(st.seen_misses > 0, "candidate misses_o fired", 1, st.seen_misses > 0);
    zhao::check(st.seen_rejected > 0, "candidate programs_rejected_o fired", 1, st.seen_rejected > 0);
    zhao::check(st.seen_evictions > 0, "candidate evictions_o fired", 1, st.seen_evictions > 0);
    zhao::check(st.seen_occupancy > 0, "candidate occupancy_o fired", 1, st.seen_occupancy > 0);
    std::printf("directed: candidate counters reached hits=%u misses=%u rejected=%u evictions=%u occupancy=%u\n",
                st.seen_hits, st.seen_misses, st.seen_rejected, st.seen_evictions, st.seen_occupancy);
  }

  // ---- 2. THE TIE (and wrap), where the elaboration can reach it -------------
  if (progdir::tie_reachable()) {
    run_script(h, progdir::script_tie(), "tie");
  } else {
    std::printf("tie: not reachable at LRUW=%u with legal stimulus -- run the ENTRIES=2 LRUW=2 build\n", kLruW);
  }

  // ---- 3. THE TIMING PROBE -- derived from RTL, not inherited --------------
  if (probe) {
    h.run(progdir::reset_op(progdir::hash_of(0), "probe: reset"), "probe reset");
    std::printf("\n--- timing, ENTRIES=%u (harness cycles; one cycle = one clk) ---\n", kEntries);
    for (int side = 0; side < 2; ++side) {
      const bool cand = side == 1;
      const char* nm = cand ? "candidate" : "oracle";
      char buf[96];
      // a fresh directory: lookups all MISS
      std::snprintf(buf, sizeof buf, "%s lookup (miss) stream, no stall", nm);
      print_intervals(buf, h.probe_intervals(cand, false, false, 6, 0x1000'0000u));
      // valid commits: first fills, then evicts
      std::snprintf(buf, sizeof buf, "%s valid-commit stream, no stall", nm);
      print_intervals(buf, h.probe_intervals(cand, true, true, kEntries + 4, 0x2000'0000u));
      // lookups that HIT (the hashes just committed)
      std::snprintf(buf, sizeof buf, "%s lookup (hit) stream, no stall", nm);
      print_intervals(buf, h.probe_intervals(cand, false, false, 6, 0x2000'0004u));
      // rejected commits: no scan on the candidate
      std::snprintf(buf, sizeof buf, "%s rejected-commit stream, no stall", nm);
      print_intervals(buf, h.probe_intervals(cand, true, false, 6, 0x3000'0000u));
      std::printf("MEASURED %-52s accept-to-response-visible: %ld cycles\n",
                  (std::string(nm) + " lookup (hit) latency").c_str(),
                  h.probe_latency(cand, false, false, 0x2000'0005u));
      std::printf("MEASURED %-52s accept-to-response-visible: %ld cycles\n",
                  (std::string(nm) + " lookup (miss) latency").c_str(),
                  h.probe_latency(cand, false, false, 0x4000'0000u));
      std::printf("MEASURED %-52s accept-to-response-visible: %ld cycles\n",
                  (std::string(nm) + " valid-commit (evict) latency").c_str(),
                  h.probe_latency(cand, true, true, 0x5000'0000u));
      std::printf("MEASURED %-52s accept-to-response-visible: %ld cycles\n",
                  (std::string(nm) + " rejected-commit latency").c_str(),
                  h.probe_latency(cand, true, false, 0x6000'0000u));
      h.run(progdir::reset_op(progdir::hash_of(0), "probe: reset"), "probe reset");
    }
    // The candidate's scan is a fixed walk: the two lookup intervals must agree.
    std::printf("--- end timing ---\n\n");
  }

  std::printf("directed: %u ops compared; candidate max latency lookup %ld commit %ld cycles\n",
              h.stats().ops, h.stats().max_lu_latency, h.stats().max_cm_latency);

  top.final();
  return zhao::report_and_exit("field_progdir_differential");
}
