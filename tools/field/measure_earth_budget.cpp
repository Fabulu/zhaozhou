// measure_earth_budget.cpp — DOES EARTH FIT IN 850,000 CLOCKS?
//
// COMMITTED, like its two siblings, because this number decides what gets
// built next and a figure nobody can recheck is not evidence.
//
// ---------------------------------------------------------------------------
// THE LAW BEING APPLIED
// ---------------------------------------------------------------------------
// spec/form/cost-model.md, "Admission law":
//
//   A program is `realtime/hot` for a profile if and only if every component
//   of its demand vector fits the MEASURED machine -- measured service
//   initiation intervals, measured issue rate, measured clock -- inside the
//   profile's deadline (Earth: <= 6,000 clocks/association, <= 850,000 clocks
//   for the 128-association stress frame). An opcode count below the ceiling
//   certifies nothing by itself.
//
// So this tool does exactly that and nothing cleverer: it plans the shipped
// Earth programs, and multiplies each vector uop by the cost that block was
// MEASURED to take on the composed machine.
//
// ---------------------------------------------------------------------------
// WHERE THE COSTS COME FROM
// ---------------------------------------------------------------------------
// The `MEASURED:` lines of tests/differential/field_v3_full_directed.cpp, run
// against the real RTL through the whole engine -- executor, dispatcher,
// service path, bank and writeback. They are end-to-end group latencies, not
// datasheet numbers and not estimates.
//
// If those change, this table must change with them, and the differential
// prints them on every run so the drift is visible rather than silent.
//
// ---------------------------------------------------------------------------
// WHAT THIS DELIBERATELY DOES NOT CLAIM
// ---------------------------------------------------------------------------
// It sums the per-group costs SERIALLY. The dispatcher now keeps two groups in
// flight and there are seven services, so real overlap exists and the true
// figure is somewhere at or below this one.
//
// A serial sum is therefore an UPPER BOUND, and it is the honest thing to
// report first: if the bound fits, the program certainly fits; if the bound
// misses by a factor of six, no amount of overlap closes that, because the
// services that dominate all contend for the same multiplier bank and the same
// single root unit.
//
// Reporting an overlap-adjusted figure before overlap has been measured would
// be exactly the "true and useless number" this project keeps refusing to
// produce.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"

namespace {

// spec/form/cost-model.md: 273 four-point groups make one 1,089-vertex
// association, and the Earth stress frame is 128 associations.
constexpr int kGroupsPerAssociation = 273;
constexpr int kAssociationsPerFrame = 128;
constexpr long kFrameBudget = 850000;
constexpr long kAssociationBudget = 6000;

/** MEASURED INITIATION INTERVAL, in clocks per four-point group.
 *
 *  II, NOT LATENCY. The admission law asks for "measured service initiation
 *  intervals" and this tool first used latencies, which reported Earth 9.1x
 *  over. That was an upper bound and it was labelled as one, but for the six
 *  services that gated ready on being idle the two numbers were the same and
 *  the bound was tight. Pipelining them separated the two, so the figures below
 *  are streamed measurements: N groups back to back, elapsed clocks divided by
 *  groups RETIRED.
 *
 *  Sources: each block's own `MEASURED: ... II = N clocks/group` line.
 */
int cost_of(uint8_t op) {
  switch (op) {
    // ---- pipelined, streamed measurements --------------------------------
    case zfield::OP_CURVE: return 13;        // curve service, II gate
    case zfield::OP_DCURVE: return 13;
    case zfield::OP_SPLINE: return 13;
    case zfield::OP_SIN: return 4;           // trig, streamed
    case zfield::OP_COS: return 4;
    case zfield::UOP_RING_PREP: return 19;   // two ring units
    case zfield::OP_LEN2: return 22;         // two banks of four roots
    case zfield::OP_LEN3: return 22;
    case zfield::OP_DIST2: return 22;
    // ---- STILL BLOCKING: v_ready gated on idle, so II = latency ----------
    case zfield::OP_RIDGE: return 29;
    case zfield::OP_NOISE2: return 36;
    case zfield::OP_NORMALIZE2: return 182;
    case zfield::OP_ROT2: return 39;
    case zfield::OP_NORMALIZE3: return 189;
    case zfield::OP_ROT3: return 40;
    // ---- the ALU: one instruction per clock through the pipe --------------
    // Short ops retire at issue rate. Counted as 1 rather than 0 so a program
    // made entirely of them still has a cost.
    default: return 1;
  }
}

bool is_service(uint8_t op) { return cost_of(op) > 1; }

const char* op_name(uint8_t op) {
  switch (op) {
    case zfield::OP_MOV: return "MOV";
    case zfield::OP_LDC: return "LDC";
    case zfield::OP_ADD: return "ADD";
    case zfield::OP_SUB: return "SUB";
    case zfield::OP_MUL: return "MUL";
    case zfield::OP_MAD: return "MAD";
    case zfield::OP_MIN: return "MIN";
    case zfield::OP_MAX: return "MAX";
    case zfield::OP_ABS: return "ABS";
    case zfield::OP_CLAMP: return "CLAMP";
    case zfield::OP_SELECT: return "SELECT";
    case zfield::OP_CMP: return "CMP";
    case zfield::OP_DOT2: return "DOT2";
    case zfield::OP_DOT3: return "DOT3";
    case zfield::OP_LEN2: return "LEN2";
    case zfield::OP_LEN3: return "LEN3";
    case zfield::OP_DIST2: return "DIST2";
    case zfield::OP_NORMALIZE2: return "NORMALIZE2";
    case zfield::OP_NORMALIZE3: return "NORMALIZE3";
    case zfield::OP_SIN: return "SIN";
    case zfield::OP_COS: return "COS";
    case zfield::OP_CURVE: return "CURVE";
    case zfield::OP_SPLINE: return "SPLINE";
    case zfield::OP_NOISE2: return "NOISE2";
    case zfield::OP_DCURVE: return "DCURVE";
    case zfield::OP_RIDGE: return "RIDGE";
    case zfield::OP_ROT2: return "ROT2";
    case zfield::OP_ROT3: return "ROT3";
    case zfield::UOP_RING_PREP: return "RING_PREP";
    default: return "?";
  }
}

std::vector<uint8_t> slurp(const char* path) {
  std::vector<uint8_t> v;
  FILE* f = fopen(path, "rb");
  if (!f) return v;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  v.resize((size_t)(n > 0 ? n : 0));
  if (!v.empty() && fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
  fclose(f);
  return v;
}

const char* base(const char* p) {
  const char* s = strrchr(p, '/');
  const char* b = strrchr(p, '\\');
  if (b > s) s = b;
  return s ? s + 1 : p;
}

}  // namespace

int main(int argc, char** argv) {
  printf("Earth admission, against %ld clocks for %d associations "
         "(%ld per association)\n\n",
         kFrameBudget, kAssociationsPerFrame, kAssociationBudget);

  long worst_frame = 0;
  const char* worst_name = "(none)";
  std::map<uint8_t, long> frame_by_op;

  for (int i = 1; i < argc; ++i) {
    const std::vector<uint8_t> bytes = slurp(argv[i]);
    if (bytes.empty()) continue;
    const zfield::DecodeResult dec = zfield::decode(bytes.data(), bytes.size());
    if (dec.error != zfield::DecodeError::kOk) {
      printf("%-16s UNDECODABLE\n", base(argv[i]));
      continue;
    }
    const zfield::Fplan fp = zfield::plan(dec.prog, 0b11);  // Earth: x,z vary

    long per_group = 0;
    std::map<uint8_t, long> by_op;
    for (const zfield::VecUop& v : fp.uops) {
      const long c = cost_of(v.op);
      per_group += c;
      by_op[v.op] += c;
    }
    // TWO BOUNDS, because one of them is a projection and saying which is the
    // difference between a measurement and a hope.
    //
    // SERIAL sums every uop's II: what the machine costs if nothing overlaps.
    // OVERLAPPED takes the busiest single service: what it costs if every
    // service runs concurrently and the group rate is set by whichever is most
    // loaded. The truth is between them, and WHERE between them is a composed
    // measurement this tool cannot make.
    long busiest = 0;
    for (const auto& kv : by_op)
      if (kv.second > busiest) busiest = kv.second;
    const long ov_group = busiest;
    const long ov_frame = ov_group * kGroupsPerAssociation * kAssociationsPerFrame;

    const long per_assoc = per_group * kGroupsPerAssociation;
    const long per_frame = per_assoc * kAssociationsPerFrame;
    printf("%-16s  OVERLAPPED group %4ld  frame %9ld %s\n", base(argv[i]), ov_group,
           ov_frame, ov_frame <= kFrameBudget ? "FITS" : "OVER");
    if (per_frame > worst_frame) {
      worst_frame = per_frame;
      worst_name = base(argv[i]);
    }
    for (const auto& kv : by_op)
      frame_by_op[kv.first] += kv.second * kGroupsPerAssociation * kAssociationsPerFrame;

    printf("%-16s  group %5ld   association %8ld %s   frame %10ld %s\n", base(argv[i]),
           per_group, per_assoc, per_assoc <= kAssociationBudget ? "OK  " : "OVER",
           per_frame, per_frame <= kFrameBudget ? "OK" : "OVER");

    // Where the clocks actually go, biggest first, so the fix is obvious.
    std::vector<std::pair<long, uint8_t>> rank;
    for (const auto& kv : by_op) rank.push_back({kv.second, kv.first});
    std::sort(rank.rbegin(), rank.rend());
    for (size_t r = 0; r < rank.size() && r < 4; ++r)
      printf("                    %-12s %5ld clocks/group  (%2ld%%)\n", op_name(rank[r].second),
             rank[r].first, per_group ? (100 * rank[r].first / per_group) : 0);
  }

  printf("\n== the worst program ==\n");
  printf("   %s: %ld clocks per frame against %ld  -> %s by %.1fx\n", worst_name, worst_frame,
         kFrameBudget, worst_frame <= kFrameBudget ? "FITS" : "OVER",
         kFrameBudget ? (double)worst_frame / (double)kFrameBudget : 0.0);

  printf("\n== where the frame time goes, all programs summed ==\n");
  std::vector<std::pair<long, uint8_t>> rank;
  long total = 0;
  for (const auto& kv : frame_by_op) {
    rank.push_back({kv.second, kv.first});
    total += kv.second;
  }
  std::sort(rank.rbegin(), rank.rend());
  for (const auto& r : rank)
    printf("   %-12s %12ld  (%2ld%%)%s\n", op_name(r.second), r.first,
           total ? (100 * r.first / total) : 0,
           is_service(r.second) ? "" : "   [ALU]");

  return 0;
}
