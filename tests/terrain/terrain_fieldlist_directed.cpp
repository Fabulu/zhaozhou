// terrain_fieldlist_directed.cpp -- TERRAIN.FIELDLIST, the sealed per-frame
// TerrainField association list and its per-patch replay.
//
// WHY THE BLOCK EXISTS, in one line, because it decides what this bench has to
// discriminate: `zhao_cmd_exec` publishes TerrainField 0x0200 records ONCE PER
// COMMAND PACKET and drains them once, while `zhao_terrain_patch`'s
// `list_clear_i` fires ONCE PER PATCH JOB and empties the list. A frame has
// many patch jobs. So the fault this block prevents is not "the record never
// arrives" -- it is "the record arrives for the FIRST patch of the frame and
// for no other", silently, with every counter on both blocks balancing.
//
// WHAT ACTUALLY DISCRIMINATES HERE, named up front, because a bench that shows
// one record going in and one coming out would pass against a pure wire:
//
//   1. THE SECOND REPLAY. A pass-through delivers the frame's list to the
//      first patch job and nothing to the second. Every replay case below
//      pulses `patch_open_i` TWICE and requires the SAME records, in the SAME
//      command order, both times. This is the whole point of the block and it
//      is the first check.
//
//   2. COMMAND ORDER, WITH DISTINGUISHABLE PAYLOADS. Every record carries its
//      own footprint and its own cmd index, so a replay that reordered, that
//      presented entry 0's footprint under entry 2's index, or that emitted
//      one entry N times, fails. Records that differ only in a field nobody
//      reads would discriminate nothing.
//
//   3. THE SEAL BLOCKS A REPLAY, AND DOES NOT DEADLOCK IT. A patch job taken
//      while the set is still arriving must NOT replay a partial prefix: it
//      waits, `open_at_patch_o` counts it, intake KEEPS BEING ACCEPTED (if it
//      did not, the record carrying the seal could never arrive and the
//      console would hang), and the replay then delivers the WHOLE list.
//
//   4. THE STALL IS ASSERTED FOR THE WHOLE REPLAY. `patch_stall_o` is what
//      holds the vertex lane in the console. If it dropped early the patch
//      would begin composing against a half-filled section 9.1 list -- the
//      same class of fault as the cadence fault itself, one level down.
//
//   5. THE HASH IS RESOLVED, NOT FORWARDED. A handle published by a READY
//      object resolves to THAT OBJECT's program hash; a handle published only
//      by a NOT-READY object does not resolve at all. Both are checked, and
//      the second is the one that separates a real sweep from a mux that
//      ignores `pub_ready_i`. The resolved hash must never equal the handle --
//      forwarding the handle into a field called hash is the specific lie
//      `zhao_cmd_exec`'s header warns about.
//
//   6. THE TAIL IS REJECTED AND NOTHING IS EVICTED (terrain_rules 9.1 law 2).
//      Seventeen records in one set leave sixteen, the SEVENTEENTH is the one
//      missing, and `tail_rejected_o` moves once. A block that evicted the
//      head would also leave sixteen and would also move no other counter.
//
//   7. THE EMPTY CASE COSTS NOTHING. With no record ever delivered, a patch
//      job must retire with `add_valid_o` never rising and `replays_o` moving.
//      That case is EVERY EXISTING CONSOLE SMOKE FORM, so a block that
//      deadlocked here would take the whole terrain chain down and no existing
//      test would say why.
//
//   8. EVERY COUNTER IS ASSERTED SILENT AND THEN FIRED (R95). A counter whose
//      zero nobody has seen move is a claim, not a measurement. All six are
//      driven from legal stimulus at this module's own port, so none of them
//      needs a committed mutant.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_terrain_fieldlist.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

namespace {

constexpr int kObjects = 8;

struct Rec {
  int32_t x0, z0, x1, z1;
  uint32_t handle;
  uint16_t cmd;
};

struct Seen {
  int32_t x0, z0, x1, z1;
  uint32_t hash;
  uint16_t cmd;
};

// The publication table the DUT sweeps. `ready` is a MASK bit, deliberately
// separate from the handle, so a sweep that ignores it is visible.
struct PubObj {
  uint32_t handle;
  uint32_t prog_hash;
  bool ready;
};

PubObj g_pub[kObjects];

// Drive the combinational publication mux for whatever `pub_sel_o` currently
// asks for. Called after every `eval()` so the DUT sees the object it selected
// in the same cycle -- which is what the real `zhao_field_loader` does
// (`assign pub_handle_o = obj_handle32[pub_sel_i];`).
void drive_pub(Vzhao_terrain_fieldlist& d) {
  uint32_t mask = 0;
  for (int i = 0; i < kObjects; ++i)
    if (g_pub[i].ready) mask |= (1u << i);
  d.pub_ready_i = mask;
  const int sel = d.pub_sel_o & (kObjects - 1);
  d.pub_handle_i = g_pub[sel].handle;
  d.pub_prog_hash_i = g_pub[sel].prog_hash;
}

void step(Vzhao_terrain_fieldlist& d) {
  d.eval();
  drive_pub(d);
  d.eval();
  tick(d);
  d.eval();
  drive_pub(d);
  d.eval();
}

void hard_reset(Vzhao_terrain_fieldlist& d) {
  d.rst_n = 0;
  d.cmd_valid_i = 0;
  d.cmd_x0_i = 0;
  d.cmd_z0_i = 0;
  d.cmd_x1_i = 0;
  d.cmd_z1_i = 0;
  d.cmd_handle_i = 0;
  d.cmd_cmd_i = 0;
  d.cmd_last_i = 0;
  d.patch_open_i = 0;
  d.add_ready_i = 1;
  for (int i = 0; i < 4; ++i) step(d);
  d.rst_n = 1;
  for (int i = 0; i < 2; ++i) step(d);
}

// Offer one record and wait for `cmd_ready_o`. Returns false if the block
// never accepted it -- which is the deadlock this bench exists partly to
// refuse, so it is reported rather than hung on.
bool offer(Vzhao_terrain_fieldlist& d, const Rec& r, bool last, int budget = 400) {
  d.cmd_valid_i = 1;
  d.cmd_x0_i = r.x0;
  d.cmd_z0_i = r.z0;
  d.cmd_x1_i = r.x1;
  d.cmd_z1_i = r.z1;
  d.cmd_handle_i = r.handle;
  d.cmd_cmd_i = r.cmd;
  d.cmd_last_i = last ? 1 : 0;
  // The record RETIRES at the end of the publication sweep, not at the
  // handshake -- so a bench that read `records_o` the cycle after the take
  // would read it OBJECTS clocks early and see the list one short. Waiting on
  // the retirement COUNTERS is exact and needs no knowledge of the sweep's
  // length; waiting a fixed number of clocks would encode one.
  const uint32_t retired_before = d.records_sealed_o + d.tail_rejected_o;
  bool taken_once = false;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    drive_pub(d);
    d.eval();
    const bool taken = !taken_once && (d.cmd_ready_o != 0);
    step(d);
    if (taken) {
      taken_once = true;
      d.cmd_valid_i = 0;
      d.cmd_last_i = 0;
    }
    if (taken_once && (d.records_sealed_o + d.tail_rejected_o) != retired_before)
      return true;
  }
  d.cmd_valid_i = 0;
  d.cmd_last_i = 0;
  return false;
}

// One patch job: pulse `patch_open_i`, then run until the replay retires,
// capturing every record the block offers. `stall_held` reports whether
// `patch_stall_o` stayed high for the whole of it.
std::vector<Seen> replay(Vzhao_terrain_fieldlist& d, bool* stall_held,
                         int budget = 600) {
  std::vector<Seen> out;
  const uint32_t replays_before = d.replays_o;
  d.patch_open_i = 1;
  step(d);
  d.patch_open_i = 0;

  bool held = true;
  for (int i = 0; i < budget; ++i) {
    d.eval();
    drive_pub(d);
    d.eval();
    if (d.add_valid_o && d.add_ready_i) {
      Seen s;
      s.x0 = d.add_x0_o;
      s.z0 = d.add_z0_o;
      s.x1 = d.add_x1_o;
      s.z1 = d.add_z1_o;
      s.hash = d.add_hash_o;
      s.cmd = d.add_cmd_o;
      out.push_back(s);
    }
    const bool done = (d.replays_o != replays_before);
    if (!done && !d.patch_stall_o) held = false;
    step(d);
    if (done) break;
  }
  if (stall_held) *stall_held = held;
  return out;
}

void expect_list(const char* what, const std::vector<Seen>& got,
                 const std::vector<Rec>& want, const std::vector<uint32_t>& hashes) {
  char buf[160];
  std::snprintf(buf, sizeof buf, "%s: replay length", what);
  check(got.size() == want.size(), buf, want.size(), got.size());
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    std::snprintf(buf, sizeof buf, "%s: entry %zu x0", what, i);
    check(got[i].x0 == want[i].x0, buf, (uint64_t)(uint32_t)want[i].x0,
          (uint64_t)(uint32_t)got[i].x0);
    std::snprintf(buf, sizeof buf, "%s: entry %zu z1", what, i);
    check(got[i].z1 == want[i].z1, buf, (uint64_t)(uint32_t)want[i].z1,
          (uint64_t)(uint32_t)got[i].z1);
    std::snprintf(buf, sizeof buf, "%s: entry %zu cmd", what, i);
    check(got[i].cmd == want[i].cmd, buf, want[i].cmd, got[i].cmd);
    std::snprintf(buf, sizeof buf, "%s: entry %zu hash", what, i);
    check(got[i].hash == hashes[i], buf, hashes[i], got[i].hash);
  }
}

Rec mk(int n) {
  Rec r;
  r.x0 = 0x00010000 * n;
  r.z0 = 0x00020000 * n + 7;
  r.x1 = 0x00030000 * n + 11;
  r.z1 = -(0x00040000 * n + 13);
  r.handle = 0xA0000000u + (uint32_t)n;
  r.cmd = (uint16_t)(0x0500 + n);
  return r;
}

void clear_pub() {
  for (int i = 0; i < kObjects; ++i) {
    g_pub[i].handle = 0xDEAD0000u + (uint32_t)i;
    g_pub[i].prog_hash = 0xBEEF0000u + (uint32_t)i;
    g_pub[i].ready = false;
  }
}

}  // namespace

int main() {
  Vzhao_terrain_fieldlist dut;
  clear_pub();
  hard_reset(dut);

  // ------------------------------------------------------------------------
  // 7. THE EMPTY CASE FIRST, because it is every existing console smoke form.
  // ------------------------------------------------------------------------
  check(dut.sealed_o == 1, "reset: the list is SEALED and empty", 1, dut.sealed_o);
  check(dut.records_o == 0, "reset: no records", 0, dut.records_o);
  check(dut.records_sealed_o == 0, "counter records_sealed silent at reset", 0,
        dut.records_sealed_o);
  check(dut.tail_rejected_o == 0, "counter tail_rejected silent at reset", 0,
        dut.tail_rejected_o);
  check(dut.unresolved_o == 0, "counter unresolved silent at reset", 0,
        dut.unresolved_o);
  check(dut.replays_o == 0, "counter replays silent at reset", 0, dut.replays_o);
  check(dut.entries_replayed_o == 0, "counter entries_replayed silent at reset", 0,
        dut.entries_replayed_o);
  check(dut.open_at_patch_o == 0, "counter open_at_patch silent at reset", 0,
        dut.open_at_patch_o);

  {
    bool held = false;
    std::vector<Seen> got = replay(dut, &held);
    check(got.empty(), "empty list: nothing is offered to the 9.1 intake", 0,
          got.size());
    check(dut.replays_o == 1, "empty list: the replay still RETIRES", 1,
          dut.replays_o);
    check(dut.entries_replayed_o == 0, "empty list: no entries replayed", 0,
          dut.entries_replayed_o);
  }

  // ------------------------------------------------------------------------
  // 5. THE HASH IS RESOLVED, NOT FORWARDED -- set the publication table up so
  //    one handle is READY, one is present but NOT READY, and one is absent.
  // ------------------------------------------------------------------------
  const Rec r0 = mk(1);   // resolves: object 3, ready
  const Rec r1 = mk(2);   // present at object 5 but NOT ready -> unresolved
  const Rec r2 = mk(3);   // in no object at all -> unresolved
  g_pub[3].handle = r0.handle;
  g_pub[3].prog_hash = 0x1234ABCDu;
  g_pub[3].ready = true;
  g_pub[5].handle = r1.handle;
  g_pub[5].prog_hash = 0x99999999u;
  g_pub[5].ready = false;

  check(offer(dut, r0, false), "intake accepts record 0", 1, 1);
  check(dut.sealed_o == 0, "a non-last record leaves the list OPEN", 0, dut.sealed_o);
  check(offer(dut, r1, false), "intake accepts record 1", 1, 1);

  // ------------------------------------------------------------------------
  // 3. A PATCH JOB TAKEN MID-SET WAITS, COUNTS, AND DOES NOT DEADLOCK.
  // ------------------------------------------------------------------------
  dut.patch_open_i = 1;
  step(dut);
  dut.patch_open_i = 0;
  step(dut);
  check(dut.open_at_patch_o == 1, "a patch job met an UNSEALED list and it is counted",
        1, dut.open_at_patch_o);
  check(dut.patch_stall_o == 1, "the vertex lane is HELD while the list is open", 1,
        dut.patch_stall_o);
  check(dut.add_valid_o == 0, "no PARTIAL prefix is offered before the seal", 0,
        dut.add_valid_o);
  // The record carrying the seal must still be accepted. If intake were refused
  // while a replay is pending, this is where the console would hang forever.
  check(offer(dut, r2, true), "intake is STILL ACCEPTED while a replay is pending",
        1, 1);
  check(dut.sealed_o == 1, "the last record SEALS the list", 1, dut.sealed_o);
  check(dut.records_o == 3, "three records in the sealed list", 3, dut.records_o);
  check(dut.records_sealed_o == 3, "counter records_sealed FIRED", 3,
        dut.records_sealed_o);
  check(dut.unresolved_o == 2, "counter unresolved FIRED, twice", 2, dut.unresolved_o);

  const std::vector<Rec> want = {r0, r1, r2};
  const std::vector<uint32_t> hashes = {0x1234ABCDu, 0u, 0u};

  // The pending replay is released by the seal and delivers the WHOLE list.
  {
    bool held = false;
    std::vector<Seen> got;
    const uint32_t before = dut.replays_o;
    for (int i = 0; i < 600; ++i) {
      dut.eval();
      drive_pub(dut);
      dut.eval();
      if (dut.add_valid_o && dut.add_ready_i) {
        Seen s;
        s.x0 = dut.add_x0_o; s.z0 = dut.add_z0_o;
        s.x1 = dut.add_x1_o; s.z1 = dut.add_z1_o;
        s.hash = dut.add_hash_o; s.cmd = dut.add_cmd_o;
        got.push_back(s);
      }
      const bool done = (dut.replays_o != before);
      if (!done && !dut.patch_stall_o) held = false;
      step(dut);
      if (done) break;
    }
    expect_list("mid-set job, released by the seal", got, want, hashes);
    check(got.size() == 3 && got[0].hash != r0.handle,
          "the resolved hash is the OBJECT's, never the handle", 1,
          (got.size() == 3 && got[0].hash != r0.handle) ? 1 : 0);
  }

  // ------------------------------------------------------------------------
  // 1. THE SECOND AND THIRD REPLAYS -- the fault this block exists to prevent.
  // ------------------------------------------------------------------------
  for (int job = 2; job <= 3; ++job) {
    bool held = false;
    std::vector<Seen> got = replay(dut, &held);
    char buf[96];
    std::snprintf(buf, sizeof buf, "patch job %d of the SAME frame", job);
    expect_list(buf, got, want, hashes);
    std::snprintf(buf, sizeof buf, "patch job %d: the stall is held for the WHOLE replay",
                  job);
    check(held, buf, 1, held ? 1 : 0);
  }
  check(dut.entries_replayed_o == 9,
        "counter entries_replayed FIRED: three records over three patch jobs", 9,
        dut.entries_replayed_o);

  // ------------------------------------------------------------------------
  // A NEW FRAME REPLACES THE LIST, it does not append to it.
  // ------------------------------------------------------------------------
  const Rec n0 = mk(9);
  check(offer(dut, n0, true), "intake accepts the next frame's only record", 1, 1);
  check(dut.records_o == 1, "a new frame's first record REPLACES the sealed list", 1,
        dut.records_o);
  {
    bool held = false;
    std::vector<Seen> got = replay(dut, &held);
    const std::vector<Rec> w2 = {n0};
    const std::vector<uint32_t> h2 = {0u};
    expect_list("the next frame", got, w2, h2);
  }

  // ------------------------------------------------------------------------
  // 6. THE TAIL IS REJECTED AND NOTHING IS EVICTED.
  // ------------------------------------------------------------------------
  hard_reset(dut);
  clear_pub();
  std::vector<Rec> big;
  for (int i = 0; i < 17; ++i) big.push_back(mk(100 + i));
  for (int i = 0; i < 17; ++i)
    check(offer(dut, big[i], i == 16), "tail case: record offered", 1, 1);
  check(dut.records_o == 16, "the list holds exactly MAX_FIELDS", 16, dut.records_o);
  check(dut.tail_rejected_o == 1, "counter tail_rejected FIRED, once", 1,
        dut.tail_rejected_o);
  check(dut.records_sealed_o == 16, "only the accepted records are counted sealed", 16,
        dut.records_sealed_o);
  {
    bool held = false;
    std::vector<Seen> got = replay(dut, &held);
    check(got.size() == 16, "tail case: sixteen entries replayed", 16, got.size());
    if (got.size() == 16) {
      // The HEAD survived -- an evicting list would also hold sixteen.
      check(got[0].cmd == big[0].cmd, "tail case: the HEAD survived (never evict)",
            big[0].cmd, got[0].cmd);
      check(got[15].cmd == big[15].cmd, "tail case: entry 15 is record 15",
            big[15].cmd, got[15].cmd);
      bool tail_present = false;
      for (const Seen& s : got)
        if (s.cmd == big[16].cmd) tail_present = true;
      check(!tail_present, "tail case: the SEVENTEENTH record is the one missing", 0,
            tail_present ? 1 : 0);
    }
  }

  // ------------------------------------------------------------------------
  // 4. BACKPRESSURE: the consumer's ready is a constant 1 in today's console,
  //    and a lane that only worked against a constant ready is a lane that
  //    breaks the day it stops being one.
  // ------------------------------------------------------------------------
  hard_reset(dut);
  clear_pub();
  g_pub[0].handle = mk(21).handle;
  g_pub[0].prog_hash = 0x0F0F0F0Fu;
  g_pub[0].ready = true;
  const Rec b0 = mk(21);
  const Rec b1 = mk(22);
  check(offer(dut, b0, false), "backpressure case: record 0", 1, 1);
  check(offer(dut, b1, true), "backpressure case: record 1", 1, 1);

  dut.add_ready_i = 0;
  dut.patch_open_i = 1;
  step(dut);
  dut.patch_open_i = 0;
  for (int i = 0; i < 20; ++i) step(dut);
  check(dut.add_valid_o == 1, "backpressure: the record is HELD, not dropped", 1,
        dut.add_valid_o);
  check(dut.entries_replayed_o == 0, "backpressure: nothing was retired", 0,
        dut.entries_replayed_o);
  check(dut.patch_stall_o == 1, "backpressure: the vertex lane is still held", 1,
        dut.patch_stall_o);
  const int32_t held_x0 = dut.add_x0_o;
  check(held_x0 == b0.x0, "backpressure: the held record is still entry 0",
        (uint64_t)(uint32_t)b0.x0, (uint64_t)(uint32_t)held_x0);
  dut.add_ready_i = 1;
  for (int i = 0; i < 40; ++i) {
    step(dut);
    if (!dut.patch_stall_o) break;
  }
  check(dut.entries_replayed_o == 2, "backpressure: both records retire once ready", 2,
        dut.entries_replayed_o);
  check(dut.patch_stall_o == 0, "backpressure: the stall releases", 0,
        dut.patch_stall_o);

  return zhao::report_and_exit("terrain_fieldlist_directed");
}
