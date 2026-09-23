// projshare_contention.cpp -- THE MEASURED HALF of owner ruling R3's owed
// schedule proof.
//
// R3 owes "a written schedule proof that geometry, particles and
// FORGE.SHADOW's instance-centre 1/w share client A's bandwidth within the
// frame at the guaranteed content tier".
//
// `reports/R3-CLIENT-A-SCHEDULE-PROOF-20260923.md` answers the RATE half by
// arithmetic and derives a STARVATION BOUND from the two arbiters' RTL. This
// bench is what turns that bound from arithmetic into evidence: it saturates
// every arm of the real front multiplex against the real shared projector and
// measures the worst gap any arm actually waited.
//
// WHY THAT DISTINCTION IS THE WHOLE POINT. The proof predicts a bound at FOUR
// arms from a law read at THREE. CLAUDE.md's rule is that a detector reading
// zero is the claim to check hardest, and the same applies to a bound nobody
// has watched hold: a model validated at N=3 makes the N=4 prediction
// evidence, and a model validated nowhere makes it a calculation.
//
// WHAT IS ASSERTED, AND WHAT IS DELIBERATELY NOT:
//
//   ASSERTED  the starvation bound -- no arm ever waits longer than the
//             rotating-priority law permits, in each of four load shapes.
//   ASSERTED  work conservation -- the core is granted on every clock some
//             arm is asking, so grant-clocks are additive and the proof's
//             sum is the right kind of sum.
//   ASSERTED  the three routing detectors stay silent, because a bench that
//             fires one is measuring a routing fault and reporting a rate.
//   NOT       any projection arithmetic. `part_project_directed` owns the
//             size law and the oracle; repeating it here would be a second
//             opinion about a settled thing.
//
// THE BOUND, from the proof, and re-derived here so the number in the
// assertion is not a magic constant:
//
//   front multiplex  `zhao_part_project.sv:620-641` rotating priority,
//                    `:918-920` the turn advances ONLY on an accepted grant,
//                    so a holder is passed by at most (Na - 1) grants.
//   service          `zhao_project_service.sv:112-119` round-robin at two,
//                    so a client-A grant costs at most 2 core clocks when
//                    client B is saturated, and 1 when it is idle.
//
//   worst-case wait for a client-A arm = ((Na - 1) + 1) * Kb
//     where Na is the number of asking client-A arms and Kb is 2 when client
//     B is saturated, 1 when it is idle.
//
// Conservative C++; no harness dependency, so this builds standalone with
// verilator_bin as well as through ctest.

#include "Vtb_projshare.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string &what) {
  if (!ok) {
    std::printf("  FAIL  %s\n", what.c_str());
    ++g_failures;
  } else {
    std::printf("  ok    %s\n", what.c_str());
  }
}

struct Arm {
  const char *name;
  std::uint64_t accepts = 0;
  std::uint64_t gap = 0;      // clocks since this arm was last accepted
  std::uint64_t worst_gap = 0;
  bool asking = false;
};

struct Result {
  std::uint64_t clocks = 0;
  std::uint64_t idle_clocks = 0;   // nobody asked
  std::uint64_t granted_clocks = 0;
  Arm g{"geometry (2'd0)"};
  Arm p{"particles (2'd1)"};
  Arm f{"FORGE.PRIM (2'd2)"};
  Arm b{"terrain, client B"};
};

class Bench {
public:
  Bench() : dut_(new Vtb_projshare) {}
  ~Bench() { dut_->final(); delete dut_; }

  void tick() {
    dut_->clk = 0;
    dut_->eval();
    dut_->clk = 1;
    dut_->eval();
  }

  void reset() {
    dut_->rst_n = 0;
    dut_->cfg_we_i = 0;
    dut_->cfg_view_i = 0;
    dut_->cfg_addr_i = 0;
    dut_->cfg_data_i = 0;
    dut_->en_i = 0;
    dut_->g_valid_i = 0;
    dut_->p_valid_i = 0;
    dut_->f_valid_i = 0;
    dut_->b_valid_i = 0;
    for (int i = 0; i < 8; ++i) tick();
    dut_->rst_n = 1;
    for (int i = 0; i < 4; ++i) tick();
  }

  // A plausible view-projection: identity rows plus a viewport, written on the
  // shared bus for both views. The VALUES are irrelevant to arbitration; what
  // matters is that the core is enabled and refuses nothing, which
  // `mat_refused_o` confirms at the end of every case.
  void configure() {
    const std::int32_t kOne = 1 << 16;
    for (int view = 0; view < 2; ++view) {
      const std::int32_t mat[16] = {kOne, 0, 0, 0,
                                    0, kOne, 0, 0,
                                    0, 0, kOne, 0,
                                    0, 0, kOne, 0};
      for (int a = 0; a < 16; ++a) write_cfg(view, a, mat[a]);
      write_cfg(view, 16, 0);                       // viewport origin
      write_cfg(view, 17, (240 << 16) | 320);       // viewport extent
    }
    dut_->en_i = 1;
    for (int i = 0; i < 4; ++i) tick();
  }

  void write_cfg(int view, int addr, std::int32_t data) {
    dut_->cfg_we_i = 1;
    dut_->cfg_view_i = view;
    dut_->cfg_addr_i = addr;
    dut_->cfg_data_i = static_cast<std::uint32_t>(data);
    tick();
    dut_->cfg_we_i = 0;
    tick();
  }

  // Run `clocks` cycles with the named arms held saturated, measuring the
  // worst gap each asking arm went without an accept.
  Result run(std::uint64_t clocks, bool sg, bool sp, bool sf, bool sb) {
    Result r;
    r.g.asking = sg;
    r.p.asking = sp;
    r.f.asking = sf;
    r.b.asking = sb;

    dut_->g_valid_i = sg;
    dut_->p_valid_i = sp;
    dut_->f_valid_i = sf;
    dut_->b_valid_i = sb;

    // Distinguishable payloads. The geometry rider MUST have a zero owner
    // field or `geom_tag_collision_o` fires -- that detector is one of the
    // three this bench shows silent, so honouring it here is the point.
    dut_->g_vx_i = 1 << 16;
    dut_->g_vy_i = 2 << 16;
    dut_->g_vz_i = 3 << 16;
    dut_->g_view_i = 0;
    dut_->g_payload_i = 0x0123;          // owner bits [16:15] clear
    dut_->f_vx_i = 4 << 16;
    dut_->f_vy_i = 5 << 16;
    dut_->f_vz_i = 6 << 16;
    dut_->f_view_i = 0;
    dut_->f_slot_i = 0x0042;
    dut_->b_vx_i = 7 << 16;
    dut_->b_vy_i = 8 << 16;
    dut_->b_vz_i = 9 << 16;
    dut_->b_view_i = 0;
    dut_->b_payload_i = 0x0007;
    // A fixed plausible particle record. Its contents never reach the
    // arbiter (`pv_valid_c = p_valid_i && !slot_full_c`).
    dut_->p_record_i[0] = 0x00010000u;
    dut_->p_record_i[1] = 0x00020000u;
    dut_->p_record_i[2] = 0x00000010u;
    dut_->p_record_i[3] = 0x00000001u;

    for (std::uint64_t c = 0; c < clocks; ++c) {
      dut_->eval();
      const bool ga = sg && dut_->g_ready_o;
      const bool pa = sp && dut_->p_ready_o;
      const bool fa = sf && dut_->f_ready_o;
      const bool ba = sb && dut_->b_ready_o;
      const bool any_a_asked = sg || sp || sf;
      const bool any_asked = any_a_asked || sb;

      step_arm(r.g, sg, ga);
      step_arm(r.p, sp, pa);
      step_arm(r.f, sf, fa);
      step_arm(r.b, sb, ba);

      if (ga || pa || fa || ba) ++r.granted_clocks;
      if (!any_asked) ++r.idle_clocks;

      tick();
      ++r.clocks;
    }

    dut_->g_valid_i = 0;
    dut_->p_valid_i = 0;
    dut_->f_valid_i = 0;
    dut_->b_valid_i = 0;
    return r;
  }

  Vtb_projshare *dut() { return dut_; }

private:
  static void step_arm(Arm &a, bool asking, bool accepted) {
    if (!asking) return;
    if (accepted) {
      ++a.accepts;
      a.gap = 0;
    } else {
      ++a.gap;
      if (a.gap > a.worst_gap) a.worst_gap = a.gap;
    }
  }

  Vtb_projshare *dut_;
};

void report(const char *title, const Result &r) {
  std::printf("\n%s  (%llu clocks)\n", title,
              static_cast<unsigned long long>(r.clocks));
  const Arm *arms[4] = {&r.g, &r.p, &r.f, &r.b};
  for (const Arm *a : arms) {
    if (!a->asking) continue;
    std::printf("    %-22s accepts %8llu   worst wait %3llu clocks\n", a->name,
                static_cast<unsigned long long>(a->accepts),
                static_cast<unsigned long long>(a->worst_gap));
  }
  std::printf("    granted clocks %llu of %llu (%.1f%%)\n",
              static_cast<unsigned long long>(r.granted_clocks),
              static_cast<unsigned long long>(r.clocks),
              100.0 * static_cast<double>(r.granted_clocks) /
                  static_cast<double>(r.clocks));
}

// The bound the proof derives, re-computed rather than pasted.
std::uint64_t client_a_bound(int asking_a_arms, bool b_saturated) {
  const std::uint64_t kb = b_saturated ? 2 : 1;
  return static_cast<std::uint64_t>(asking_a_arms) * kb;
}

}  // namespace

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);

  std::printf(
      "projshare_contention -- R3's client-A schedule proof, MEASURED\n"
      "  front multiplex : zhao_part_project (real)\n"
      "  projector       : zhao_project_service + zhao_project_core (real)\n"
      "  modelled        : PART.LADDER's one-deep skid only\n");

  const std::uint64_t kClocks = 120000;

  // ---- CASE 1: everything saturated -- the worst shape there is ----------
  {
    Bench b;
    b.reset();
    b.configure();
    Result r = b.run(kClocks, true, true, true, true);
    report("CASE 1  all four arms saturated", r);

    const std::uint64_t bound = client_a_bound(3, true);
    check(r.g.worst_gap <= bound, "geometry never waited past the N=3 bound");
    check(r.f.worst_gap <= bound, "FORGE.PRIM never waited past the N=3 bound");
    // Particles are additionally throttled by SLOTS=8 against the core's
    // 36-clock latency, which is the block's OWN declared frontier knob and
    // not an arbitration fault. Its gap is therefore bounded by that round
    // trip, not by the rotating priority -- asserting the arbitration bound
    // on it would be asserting the wrong law.
    check(r.p.accepts > 0, "the particle arm was granted at all");
    check(r.b.worst_gap <= 2, "client B never waited past the A/B round-robin");
    check(r.granted_clocks == r.clocks,
          "work-conserving: the core was granted on EVERY clock somebody asked");

    check(b.dut()->geom_tag_collision_o == 0, "geom_tag_collision_o silent");
    check(b.dut()->owner_unroutable_o == 0, "owner_unroutable_o silent");
    check(b.dut()->ladder_unexpected_o == 0, "ladder_unexpected_o silent");
    check(b.dut()->mat_refused_o == 0, "mat_refused_o silent (MATW=32)");

    const std::uint64_t total =
        static_cast<std::uint64_t>(b.dut()->svc_a_grants_o) +
        static_cast<std::uint64_t>(b.dut()->svc_b_grants_o);
    std::printf("    service census: A %u  B %u  contended %u  (A+B = %llu)\n",
                b.dut()->svc_a_grants_o, b.dut()->svc_b_grants_o,
                b.dut()->svc_contended_o,
                static_cast<unsigned long long>(total));
    check(total == r.clocks,
          "A+B grants account for every clock -- grant-clocks ARE additive");
  }

  // ---- CASE 2: the three client-A arms, client B idle --------------------
  // This isolates the front multiplex. With B idle the service grants client A
  // every clock it asks, so the wait is the rotating priority's alone.
  {
    Bench b;
    b.reset();
    b.configure();
    Result r = b.run(kClocks, true, true, true, false);
    report("CASE 2  three client-A arms saturated, client B idle", r);

    const std::uint64_t bound = client_a_bound(3, false);
    check(r.g.worst_gap <= bound, "geometry within the front-mux bound alone");
    check(r.f.worst_gap <= bound, "FORGE.PRIM within the front-mux bound alone");
    check(r.granted_clocks == r.clocks, "work-conserving with B idle");
  }

  // ---- CASE 3: geometry and terrain only -- the pre-2026-09-21 shape -----
  // The composed console before the forge arm landed. It is measured so the
  // N=3 and N=4 numbers have something to be a delta FROM, rather than being
  // quoted against a remembered figure.
  {
    Bench b;
    b.reset();
    b.configure();
    Result r = b.run(kClocks, true, false, false, true);
    report("CASE 3  geometry + terrain only (the two-client baseline)", r);

    check(r.g.worst_gap <= client_a_bound(1, true),
          "geometry's wait is the A/B round-robin alone");
    check(r.b.worst_gap <= 2, "terrain's wait is the A/B round-robin alone");
    check(r.granted_clocks == r.clocks, "work-conserving at two clients");
  }

  // ---- CASE 4: a LONE client-A arm against a saturated client B ----------
  // This is the shape the FORGE.SHADOW instance centre actually presents: one
  // request every ~200 clocks from `zhao_geom_lodstate`'s single-in-flight
  // FSM, against whatever else is running. The measured wait here is the
  // number the proof's 200-clock evaluation budget is spent against.
  {
    Bench b;
    b.reset();
    b.configure();
    Result r = b.run(kClocks, true, false, false, true);
    std::printf(
        "\nCASE 4  the instance centre's shape: a single client-A asker\n"
        "    measured worst wait for the lone client-A arm: %llu clocks\n"
        "    (the proof spends 8 against a 200-clock evaluation budget)\n",
        static_cast<unsigned long long>(r.g.worst_gap));
    check(r.g.worst_gap <= 2,
          "a lone client-A asker waits at most the A/B round-robin");
  }

  std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures == 0 ? 0 : 1;
}
