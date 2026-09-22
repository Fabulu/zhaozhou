// partmat_acceptance.cpp -- THE ACCEPTANCE TEST OWNER RULING 1 NAMES, in the
// owner's own words:
//
//   "Interleave textured mesh A, a particle batch, and textured mesh B, with
//    different material sets and forced backpressure. Compare rendered output
//    against the reference. The particles draw with their own state and cause
//    no missing-material faults; both meshes preserve their state; a
//    deliberately incompatible untextured mesh is still refused and counted."
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST IS, AND -- PLAINLY -- WHAT IT IS NOT
// ---------------------------------------------------------------------------
// It is a SUBSYSTEM bench over the PRODUCTION modules in the composer's own
// arrangement: `zhao_geom_clipdoor` -> `zhao_material_window` -> R197's gate,
// with three producers and forced backpressure. Every behaviour the ruling
// names lives in exactly those blocks, and every one of them is asserted here.
//
// IT IS NOT A RENDERED-PIXEL COMPARISON. The ruling says "compare rendered
// output against the reference", and a pixel comparison needs the whole console
// with a stimulus that produces particles -- which
// `tb_zhao_console_core_smoke`'s fixture does not do (it drives meshes). Saying
// this bench proves the picture would be the "otherwise green smoke whose
// upstream fixture never reaches the new path" the same ruling warns about, so
// it is said here instead of implied. What the PIXELS still owe is recorded in
// the PARTMAT findings, which the session harness refused to write as a file --
// so they are the MESSAGE of an empty commit on `gz/partmat`, the fallback the
// packet brief names for exactly this case. `git log --grep="PARTMAT FINDINGS"`
// finds it.
//
// THE REFERENCE IS PRESENT AS THE MATERIAL MODEL. The driver plays
// MATERIAL.RESOLVE, holding each set's true record, and every published field
// is compared against the record of the set the beat actually declared -- not
// against a constant, and not against "whatever was published". That is the
// "both meshes preserve their state" clause, checked per triangle rather than
// at rest.
//
// ---------------------------------------------------------------------------
// THE THREE FAULTS THIS IS AIMED AT, ALL OF WHICH LEAVE COUNTERS HEALTHY
// ---------------------------------------------------------------------------
//   * A PARTICLE SHADED WITH A MESH'S MATERIAL, or a mesh shaded with the
//     particle span's no-sampling profile. Both are a published record that
//     belongs to a different beat, and no census counter looks at that field.
//   * A PARTICLE SILENTLY DROPPED. This is the defect that is live today:
//     R197's door refuses an untextured primitive under a sampling material,
//     the particle has no material at all, so it is refused -- with every
//     counter healthy and the particles simply never appearing.
//   * R197 QUIETLY WEAKENED to let particles through. The ruling is explicit
//     that "an untextured mesh with a sampling material does not become legal
//     merely because particles now work", so client 1 is turned into exactly
//     that mesh in section 4 and must still be refused and counted.

#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>

#include "verilated.h"

#include "Vtb_partmat_acceptance.h"

#include "zhao_sim.hpp"

namespace {

int fails = 0;
int checks = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++fails;
    std::printf("FAIL: %s -- want %llu, got %llu\n", what,
                (unsigned long long)want, (unsigned long long)got);
  }
}

constexpr int kModeBacked = 0;
constexpr int kModeNone = 1;

// What MATERIAL.RESOLVE would answer for a set. The driver plays the resolver,
// so this IS the reference for the material half.
struct Record {
  uint8_t sample_count;
  uint8_t recipe;
  uint8_t weight;
  uint8_t binding;
  uint8_t modes;        // tmu_mode; class = modes[1:0] under the default map
};

// The defined NO-SAMPLING PROFILE owner ruling 1 requires the window to select.
// Written out here rather than derived from the DUT, so the check is against
// the ruling and not against the block's own opinion of itself.
const Record kNoSampling{0, 0, 0, 0, 0};

struct Client {
  uint32_t set;
  uint16_t id;
  uint16_t src_id;
  int mode;
  bool untex;
  Record rec;
  int want;          // triangles still to offer
};

// One triangle that passed R197's gate, with the material that was published
// for it at that instant, and who produced it.
struct Drawn {
  uint16_t src_id;
  uint8_t sample_count, recipe, weight, binding, response_class, mode;
};

class Bench {
 public:
  Bench() {
    top_.clk = 0;
    top_.rst_n = 0;
    top_.c0_valid_i = 0; top_.c1_valid_i = 0; top_.c2_valid_i = 0;
    top_.cl_in_ready_i = 1;
    top_.req_ready_i = 1;
    top_.rsp_valid_i = 0;
    top_.d_reject_i = 0;
    top_.d_leave_i = 0;
    top_.eval();
    for (int i = 0; i < 4; ++i) raw_tick();
    top_.rst_n = 1;
    raw_tick();
  }

  Vtb_partmat_acceptance& top() { return top_; }

  void set_record(uint32_t set, const Record& r) { records_[set] = r; }

  void present(const Client& a, const Client& b, const Client& c) {
    drive(0, a); drive(1, b); drive(2, c);
  }

  // One clock: the resolver model, R197's observation, and the span model.
  // `sink_ready` is the forced backpressure and `pattern_` decides it.
  void tick() {
    top_.eval();

    // ---- what reached GEOM.CLIP's input on this clock -------------------
    const bool entered = top_.cl_in_valid_o && top_.cl_in_ready_i;
    if (entered) {
      Drawn d{};
      d.src_id = top_.cl_src_id_o;
      d.sample_count = top_.pub_sample_count_o;
      d.recipe = top_.pub_material_recipe_o;
      d.weight = top_.pub_recipe_weight_o;
      d.binding = top_.pub_base_binding_o;
      d.response_class = top_.pub_response_class_o;
      d.mode = top_.pub_material_mode_o;
      if (!top_.pub_valid_o) ++entered_unpublished_;
      span_.push_back(d);
      drawn_.push_back(d);
    }

    // ---- the span model: retire one every third clock --------------------
    // Deliberately SLOWER than the window can emit, so a material change waits
    // for a real drain rather than finding the span already empty.
    const bool dispose = !span_.empty() && ((clocks_ % 3) == 0);
    if (dispose) {
      const Drawn& t = span_.front();
      // THE CORRECT-BEHAVIOUR ASSERTION: the record HELD for as long as the
      // triangle was in flight. Not "a detector fired".
      if (t.sample_count != top_.pub_sample_count_o ||
          t.recipe != top_.pub_material_recipe_o ||
          t.weight != top_.pub_recipe_weight_o ||
          t.binding != top_.pub_base_binding_o ||
          t.response_class != top_.pub_response_class_o ||
          t.mode != top_.pub_material_mode_o) {
        ++inflight_mismatch_;
      }
      ++disposed_;
    }
    top_.d_leave_i = (dispose && ((disposed_ & 1) == 1)) ? 1 : 0;
    top_.d_reject_i = (dispose && ((disposed_ & 1) == 0)) ? 1 : 0;

    // ---- the resolver model ----------------------------------------------
    const bool req_fire = top_.req_valid_o && top_.req_ready_i;
    const bool rsp_fire = top_.rsp_valid_i && top_.rsp_ready_o;
    if (rsp_fire || answered_) {
      top_.rsp_valid_i = 0;
      answered_ = false;
      pending_ = -1;
    } else if (req_fire) {
      // THE REQUEST'S SET IS RECORDED. A resolve for a set nobody offered is
      // the fault that would make "the particles cost no resolve" vacuous.
      requested_.push_back(top_.req_material_set_o);
      pending_ = 3;
      ask_ = top_.req_material_set_o;
    } else if (pending_ > 0) {
      --pending_;
    } else if (pending_ == 0) {
      auto it = records_.find(ask_);
      top_.rsp_valid_i = 1;
      top_.rsp_has_record_i = (it != records_.end()) ? 1 : 0;
      const Record r = (it != records_.end()) ? it->second : Record{0, 0, 0, 0, 0};
      top_.rsp_sample_count_i = r.sample_count;
      top_.rsp_material_recipe_i = r.recipe;
      top_.rsp_recipe_weight_i = r.weight;
      top_.rsp_base_binding_i = r.binding;
      top_.rsp_sample0_modes_i = r.modes;
      answered_ = true;
    }

    // ---- FORCED BACKPRESSURE, on GEOM.CLIP's input -----------------------
    const bool sink = (pattern_ == 0) || ((clocks_ % pattern_) < 1);
    top_.cl_in_ready_i = sink ? 1 : 0;
    if (top_.cl_in_valid_o && !top_.cl_in_ready_i) ++stalled_beats_;

    raw_tick();
    if (dispose) span_.pop_front();
    top_.d_leave_i = 0;
    top_.d_reject_i = 0;
  }

  void run(int n) { for (int i = 0; i < n; ++i) tick(); }

  void quiesce(int budget = 4000) {
    top_.c0_valid_i = 0; top_.c1_valid_i = 0; top_.c2_valid_i = 0;
    for (int i = 0; i < budget && !span_.empty(); ++i) tick();
    for (int i = 0; i < 16; ++i) tick();
  }

  void set_pattern(int p) { pattern_ = p; }

  const std::deque<Drawn>& drawn() const { return drawn_; }
  const std::deque<uint32_t>& requested() const { return requested_; }
  int inflight_mismatch() const { return inflight_mismatch_; }
  int entered_unpublished() const { return entered_unpublished_; }
  int stalled_beats() const { return stalled_beats_; }
  int disposed() const { return disposed_; }

  int drawn_by(uint16_t src_id) const {
    int n = 0;
    for (const Drawn& d : drawn_) if (d.src_id == src_id) ++n;
    return n;
  }

  // Every triangle from `src_id` carried exactly `r`.
  bool state_held(uint16_t src_id, const Record& r, int mode) const {
    for (const Drawn& d : drawn_) {
      if (d.src_id != src_id) continue;
      if (d.sample_count != r.sample_count || d.recipe != r.recipe ||
          d.weight != r.weight || d.binding != r.binding ||
          d.response_class != (uint8_t)(r.modes & 3) || d.mode != mode) {
        return false;
      }
    }
    return true;
  }

 private:
  void raw_tick() {
    top_.eval();
    top_.clk = 1; top_.eval();
    top_.clk = 0; top_.eval();
    ++clocks_;
  }

  void drive(int i, const Client& c) {
    const uint8_t v = c.want > 0 ? 1 : 0;
    switch (i) {
      case 0:
        top_.c0_valid_i = v; top_.c0_material_set_i = c.set;
        top_.c0_material_id_i = c.id; top_.c0_material_mode_i = c.mode;
        top_.c0_untex_i = c.untex; top_.c0_src_id_i = c.src_id;
        break;
      case 1:
        top_.c1_valid_i = v; top_.c1_material_set_i = c.set;
        top_.c1_material_id_i = c.id; top_.c1_material_mode_i = c.mode;
        top_.c1_untex_i = c.untex; top_.c1_src_id_i = c.src_id;
        break;
      default:
        top_.c2_valid_i = v; top_.c2_material_set_i = c.set;
        top_.c2_material_id_i = c.id; top_.c2_material_mode_i = c.mode;
        top_.c2_untex_i = c.untex; top_.c2_src_id_i = c.src_id;
        break;
    }
  }

  Vtb_partmat_acceptance top_;
  std::map<uint32_t, Record> records_;
  std::deque<Drawn> span_, drawn_;
  std::deque<uint32_t> requested_;
  uint32_t ask_ = 0;
  int pending_ = -1;
  bool answered_ = false;
  uint64_t clocks_ = 0;
  int inflight_mismatch_ = 0;
  int entered_unpublished_ = 0;
  int stalled_beats_ = 0;
  int disposed_ = 0;
  int pattern_ = 0;
};

// Mesh A, the particle batch and mesh B, with DIFFERENT material sets.
const Record kRecA{1, 2, 0x5A, 0x11, 0x01};   // sampling, class NEAR
const Record kRecB{2, 5, 0xC3, 0x22, 0x02};   // sampling, class BIL

Client mesh_a(int n) { return Client{0xAAAA'0001u, 7, 0x00A0, kModeBacked, false, kRecA, n}; }
Client mesh_b(int n) { return Client{0xBBBB'0002u, 9, 0x00B0, kModeBacked, false, kRecB, n}; }
Client particles(int n) { return Client{0, 0, 0x00C0, kModeNone, true, kNoSampling, n}; }

// Drive the three clients to completion, decrementing each as its own beats are
// taken. The door's ready is per client, so this is the real producer shape.
void interleave(Bench& b, Client a, Client p, Client c, int budget = 60000) {
  for (int i = 0; i < budget && (a.want > 0 || p.want > 0 || c.want > 0); ++i) {
    b.present(a, c, p);   // door client 0 = mesh A, 1 = mesh B, 2 = particles
    b.top().eval();
    if (b.top().c0_valid_i && b.top().c0_ready_o && a.want > 0) --a.want;
    if (b.top().c1_valid_i && b.top().c1_ready_o && c.want > 0) --c.want;
    if (b.top().c2_valid_i && b.top().c2_ready_o && p.want > 0) --p.want;
    b.tick();
  }
  b.quiesce();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // ==========================================================================
  // SECTION 1 -- THE RULING'S OWN SCENARIO, WITH FORCED BACKPRESSURE.
  // ==========================================================================
  {
    Bench b;
    b.set_record(0xAAAA'0001u, kRecA);
    b.set_record(0xBBBB'0002u, kRecB);
    b.set_pattern(5);                        // GEOM.CLIP ready 1 clock in 5

    interleave(b, mesh_a(12), particles(10), mesh_b(12));

    // --- the particles drew. THIS IS THE DEFECT THE RULING EXISTS TO FIX ---
    check_eq(b.drawn_by(0x00C0), 10,
             "1: every polygon particle REACHED GEOM.CLIP's input");
    check_eq(b.drawn_by(0x00A0), 12, "1: mesh A drew");
    check_eq(b.drawn_by(0x00B0), 12, "1: mesh B drew");

    // --- ... with their OWN state, and no missing-material fault -----------
    check(b.state_held(0x00C0, kNoSampling, kModeNone),
          "1: every particle carried the DEFINED no-sampling profile in mode "
          "NO_MATERIAL -- not a mesh's record, not a fault material");
    check_eq(b.top().mw_no_record_o, 0,
             "1: NO missing-material fault for the whole run -- the ruling's own clause");
    check_eq(b.top().geom_untex_refused_o, 0,
             "1: R197's door refused NOTHING -- particles pass on the gate's own terms");
    check_eq(b.top().mw_mode_refused_o, 0,
             "1: no declaration was contradictory");

    // --- ... and both meshes preserved theirs -----------------------------
    check(b.state_held(0x00A0, kRecA, kModeBacked),
          "1: EVERY mesh A triangle carried mesh A's own resolved record");
    check(b.state_held(0x00B0, kRecB, kModeBacked),
          "1: EVERY mesh B triangle carried mesh B's own resolved record");
    check_eq(b.inflight_mismatch(), 0,
             "1: and every in-flight triangle retired under the profile it entered with");
    check_eq(b.entered_unpublished(), 0, "1: nothing entered the span unpublished");

    // --- the particle batch asked MATERIAL.RESOLVE NOTHING ----------------
    // Checked at the RESOLVER, which is an independent observer: the window's
    // own `resolves_o` and the resolver's request log are two different
    // quantities and both must agree that no particle span asked.
    bool asked_for_particle_set = false;
    for (uint32_t s : b.requested()) if (s == 0) asked_for_particle_set = true;
    check(!asked_for_particle_set,
          "1: MATERIAL.RESOLVE never saw a request for the particle span");
    check_eq(b.top().mw_resolves_o, b.requested().size(),
             "1: the window's resolve count and the resolver's request log agree");
    check(b.top().mw_no_material_spans_o >= 1,
          "1: at least one NO_MATERIAL span was published");

    // --- the stimulus reached the state -----------------------------------
    check(b.stalled_beats() > 0,
          "1: the sink ACTUALLY stalled -- otherwise the backpressure clause "
          "of the acceptance test was never exercised");
    check(b.top().cd_switches_o >= 3,
          "1: the door actually rotated between the three producers");
    // AN INDEPENDENT CENSUS OF THE SAME CLAIM. `drawn_by` counts what reached
    // GEOM.CLIP's input; `granted_o[2]` counts what the DOOR handed over. The
    // two are different registers on different sides of the material window
    // and R197's gate, so agreeing is evidence and disagreeing would name
    // exactly where the beats were lost.
    check_eq(b.top().cd_granted_o[2], 10,
             "1: the door granted the particle client exactly the ten beats "
             "that reached GEOM.CLIP -- two independent counts, one answer");
    check_eq(b.top().cd_granted_o[0], 12, "1: and twelve to mesh A");
    check_eq(b.top().cd_granted_o[1], 12, "1: and twelve to mesh B");
    check_eq(b.top().cd_err_hold_broken_o, 0, "1: the door's hold law held");
    check_eq(b.top().mw_err_unpublished_o, 0, "1: the window's interlock held");
    check_eq(b.top().mw_err_underflow_o, 0, "1: the drain accounting balanced");

    std::printf("[partmat_acceptance] A=%d particles=%d B=%d resolves=%u "
                "no_material_spans=%u stalled=%d disposed=%d\n",
                b.drawn_by(0x00A0), b.drawn_by(0x00C0), b.drawn_by(0x00B0),
                (unsigned)b.top().mw_resolves_o,
                (unsigned)b.top().mw_no_material_spans_o,
                b.stalled_beats(), b.disposed());
  }

  // ==========================================================================
  // SECTION 2 -- THE SAME, WITH THE SINK NEVER MORE THAN ONE IN ELEVEN.
  //
  // Harsher backpressure and a longer run, because the join faults this is
  // aimed at appear only while something waits.
  // ==========================================================================
  {
    Bench b;
    b.set_record(0xAAAA'0001u, kRecA);
    b.set_record(0xBBBB'0002u, kRecB);
    b.set_pattern(11);

    interleave(b, mesh_a(20), particles(20), mesh_b(20));

    check_eq(b.drawn_by(0x00C0), 20, "2: every particle drew under hard backpressure");
    check_eq(b.drawn_by(0x00A0), 20, "2: mesh A drew");
    check_eq(b.drawn_by(0x00B0), 20, "2: mesh B drew");
    check(b.state_held(0x00C0, kNoSampling, kModeNone), "2: particles kept their own state");
    check(b.state_held(0x00A0, kRecA, kModeBacked), "2: mesh A kept its own");
    check(b.state_held(0x00B0, kRecB, kModeBacked), "2: mesh B kept its own");
    check_eq(b.inflight_mismatch(), 0, "2: no in-flight profile moved");
    check_eq(b.top().mw_no_record_o, 0, "2: no missing-material fault");
    check_eq(b.top().geom_untex_refused_o, 0, "2: nothing refused");
    check(b.stalled_beats() > 0, "2: and the sink really did stall");
  }

  // ==========================================================================
  // SECTION 3 -- THE NEGATIVE CONTROL FOR THE WHOLE RULING.
  //
  // The SAME particle batch, declared MATERIAL_BACKED against a set that
  // resolves to a SAMPLING material -- which is what a particle looked like
  // before this ruling, and is the defect that is live today. R197's door must
  // refuse it and COUNT it.
  //
  // Without this section the counter in section 1 reading zero would be a claim
  // and not an instrument. With it, the same block, the same stimulus and the
  // same clock budget produce zero in one case and the exact count in the
  // other -- which is R95.
  // ==========================================================================
  {
    Bench b;
    b.set_record(0xAAAA'0001u, kRecA);
    b.set_record(0xCCCC'0003u, kRecA);   // a SAMPLING material
    b.set_pattern(5);

    Client fake = particles(10);
    fake.mode = kModeBacked;             // the pre-ruling shape
    fake.set = 0xCCCC'0003u;
    fake.id = 1;
    fake.untex = true;                   // still has no texture coordinates

    interleave(b, mesh_a(8), fake, Client{0, 0, 0, kModeBacked, false, kRecA, 0});

    check_eq(b.drawn_by(0x00C0), 0,
             "3: NOT ONE untextured beat under a sampling material reached GEOM.CLIP");
    check_eq(b.top().geom_untex_refused_o, 10,
             "3: geom_untex_refused_o FIRED, once per refused beat -- R197 intact");
    check_eq(b.drawn_by(0x00A0), 8, "3: and mesh A was unaffected");
    check(b.state_held(0x00A0, kRecA, kModeBacked), "3: with its own record");
    check_eq(b.top().mw_err_underflow_o, 0,
             "3: a refused beat never entered the span, so the drain accounting held");
    check_eq(b.top().mw_no_material_spans_o, 0,
             "3: and nothing was quietly reclassified as no-material to make it pass");
  }

  // ==========================================================================
  // SECTION 4 -- "AN UNTEXTURED MESH WITH A SAMPLING MATERIAL DOES NOT BECOME
  // LEGAL MERELY BECAUSE PARTICLES NOW WORK."
  //
  // The ruling's own sentence, as a test: a real particle batch drawing
  // correctly ON THE SAME RUN as a deliberately incompatible untextured mesh
  // that must still be refused. Section 3 shows the refusal alone; this shows
  // it CANNOT be the particles' passage that weakened the gate, because both
  // happen together.
  //
  // And the third combination R197 permits is here too: an untextured mesh
  // under a legal ZERO-SAMPLE material (`page == 255`), which must PASS. A gate
  // that refused on the untextured bit alone would fail that one.
  // ==========================================================================
  {
    Bench b;
    const Record kZeroSample{0, 1, 0x40, 0x00, 0x03};  // legal, takes no sample
    b.set_record(0xCCCC'0003u, kRecA);         // sampling -> the bad mesh
    b.set_record(0xDDDD'0004u, kZeroSample);   // non-sampling -> the legal one
    b.set_pattern(5);

    Client bad{0xCCCC'0003u, 1, 0x00D0, kModeBacked, true, kRecA, 9};
    Client good{0xDDDD'0004u, 2, 0x00E0, kModeBacked, true, kZeroSample, 7};

    interleave(b, bad, particles(9), good);

    check_eq(b.drawn_by(0x00C0), 9,
             "4: the particles drew");
    check(b.state_held(0x00C0, kNoSampling, kModeNone),
          "4: with their own no-sampling profile in mode NO_MATERIAL");
    check_eq(b.drawn_by(0x00D0), 0,
             "4: and the incompatible untextured mesh was STILL refused, on the "
             "same run, with the particles working");
    check_eq(b.top().geom_untex_refused_o, 9, "4: and counted, once per beat");
    check_eq(b.drawn_by(0x00E0), 7,
             "4: while an untextured mesh under a LEGAL zero-sample material PASSED "
             "-- the gate tests the material, not the untextured bit");
    check(b.state_held(0x00E0, kZeroSample, kModeBacked),
          "4: carrying its own resolved record, in mode MATERIAL_BACKED");
    check_eq(b.top().mw_no_record_o, 0, "4: and no missing-material fault anywhere");
    check_eq(b.inflight_mismatch(), 0, "4: every in-flight profile held");
  }

  std::printf("partmat_acceptance: %d check(s), %d failure(s)\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
