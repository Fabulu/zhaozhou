// loadq_rtl_directed.cpp -- zhao_terrain_loadq against zref::terrain::LoadQueue.
//
// ===========================================================================
// WHAT IS ACTUALLY AT RISK IN A FIFO
// ===========================================================================
// A queue is the kind of block whose test writes itself badly: push some, pop
// some, check the count, ship it. That test passes against a queue that
// reorders, against one that drops a job when full and ready lies, and against
// one that hands the loader a record with its CRC sheared off -- because none
// of those change a count.
//
// So this bench compares the SEQUENCE and the PAYLOAD, job for job and field
// for field, against the reference model, and it does it under stall patterns
// that make the interesting cycles happen:
//
//   * BOTH SIDES BUSY AT ONCE. Push and pop on the same cycle, repeatedly, at
//     every level from empty to full. That is where a level counter that
//     handles the two cases separately gets it wrong by one, and where a
//     read-during-write on the store returns the wrong word.
//   * FULL, AND OFFERED ANYWAY. `j_valid_i` held high against a full queue for
//     hundreds of cycles. Nothing may be accepted, nothing may be lost, and
//     the refusal counter must move -- it counts CYCLES, which is the number
//     that matters because it IS the sequencer's stall.
//   * EMPTY, AND DRAINED ANYWAY. A drain against nothing must be a no-op with
//     no phantom count.
//   * DRAINED MID-FLIGHT. The block serialises a job into eight M10K words on
//     the way in and out, so at any moment up to two jobs are sitting outside
//     the store -- one in the write serialiser, one in the output register. A
//     drain must throw away all three places and count all three. The first
//     version of the composed suite's drain check compared against the store's
//     level alone and was off by exactly those two, which is how this bench
//     came to have a phase for it.
//
// WHY THE MODEL DOES NOT TICK. `zref::terrain::LoadQueue` owns order, capacity
// and conservation and deliberately does not model the serialiser's latency --
// see its header. So the bench drives the RTL's handshakes and offers the model
// the same job on the cycles the RTL ACCEPTS, never blind. A model that
// mirrored the eight-cycle serialiser would be a second copy of a storage
// decision, and its first divergence would be a red test that is not a fault.
//
// THE PAYLOAD IS NEVER THE SAME TWICE. Every field of every job is distinct and
// derived from the job index, including the sign bits of ix and iz, because a
// serialiser that dropped word 6 would be invisible against a fixture of
// zeroes -- and words 6 and 7 are where the CRC and the source id live.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_loadq.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_loadq.hpp"

namespace {

namespace tq = zref::terrain;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
    std::fflush(stdout);
  }
}

void ck(bool ok, const char* what, long expect, long got) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, got);
    std::fflush(stdout);
  }
}

constexpr unsigned kDepth = 32;   // the RTL's default, which is T7's page budget
// ...and what the block can be HOLDING, which is one more: the store's 32 plus
// the one deserialised at the output port waiting for the loader. Written down
// because the first version of this bench assumed they were the same number and
// every job after the 32nd compared one position out of step.
constexpr unsigned kHold = kDepth + 1;

// EVERY FIELD DISTINCT, AND DERIVED FROM ONE INDEX. A fixture whose upper words
// are zero cannot tell a working serialiser from one that stops after word 5.
tq::LoadJob job_of(uint32_t i) {
  tq::LoadJob j;
  j.slot = i & 0x7FFu;                                    // SLOTW = 11
  j.gen = static_cast<uint8_t>((i * 7u) & 0xFFu);
  j.epoch = 0xE0000000u ^ (i * 0x01010101u);
  j.island = 0x1500'0000u + i * 0x0002'0003u;
  // BOTH SIGNS. `ix` and `iz` are signed 16-bit on the seam and a queue that
  // zero-extended them would pass every test drawn from positive coordinates.
  j.ix = static_cast<int16_t>((i % 2u) ? -static_cast<int>(i * 13u % 30000u)
                                       : static_cast<int>(i * 11u % 30000u));
  j.iz = static_cast<int16_t>((i % 3u) ? static_cast<int>(i * 17u % 30000u)
                                       : -static_cast<int>(i * 19u % 30000u));
  j.hps_addr = 0x2000'0000ull + static_cast<uint64_t>(i) * 21376ull
               + (static_cast<uint64_t>(i & 7u) << 40);   // reach the high half
  j.expect_crc = 0xC0DE'0000u ^ (i * 0x9E37'79B9u);
  j.src_id = 0x5000'0000u + i * 0x0011'0007u;
  return j;
}

void drive_job(Vzhao_terrain_loadq& d, const tq::LoadJob& j) {
  d.j_slot_i = j.slot;
  d.j_gen_i = j.gen;
  d.j_epoch_i = j.epoch;
  d.j_island_i = j.island;
  d.j_ix_i = static_cast<uint16_t>(j.ix);
  d.j_iz_i = static_cast<uint16_t>(j.iz);
  d.j_hps_addr_i = j.hps_addr;
  d.j_expect_crc_i = j.expect_crc;
  d.j_src_id_i = j.src_id;
}

tq::LoadJob out_job(Vzhao_terrain_loadq& d) {
  tq::LoadJob j;
  j.slot = d.q_slot_o;
  j.gen = static_cast<uint8_t>(d.q_gen_o);
  j.epoch = d.q_epoch_o;
  j.island = d.q_island_o;
  j.ix = static_cast<int16_t>(d.q_ix_o);
  j.iz = static_cast<int16_t>(d.q_iz_o);
  j.hps_addr = d.q_hps_addr_o;
  j.expect_crc = d.q_expect_crc_o;
  j.src_id = d.q_src_id_o;
  return j;
}

// A field-by-field report, because "job 17 differs" is not a bug report and the
// field that differs names the broken word.
int diff_report(const tq::LoadJob& got, const tq::LoadJob& want, uint32_t idx, int printed) {
  if (got == want) return printed;
  if (printed < 6) {
    std::printf("   job %u mismatch:\n", idx);
    if (got.slot != want.slot)
      std::printf("      slot       got %u want %u\n", got.slot, want.slot);
    if (got.gen != want.gen)
      std::printf("      gen        got %u want %u\n", got.gen, want.gen);
    if (got.epoch != want.epoch)
      std::printf("      epoch      got 0x%08X want 0x%08X\n", got.epoch, want.epoch);
    if (got.island != want.island)
      std::printf("      island     got 0x%08X want 0x%08X\n", got.island, want.island);
    if (got.ix != want.ix)
      std::printf("      ix         got %d want %d\n", got.ix, want.ix);
    if (got.iz != want.iz)
      std::printf("      iz         got %d want %d\n", got.iz, want.iz);
    if (got.hps_addr != want.hps_addr)
      std::printf("      hps_addr   got 0x%016llX want 0x%016llX\n",
                  (unsigned long long)got.hps_addr, (unsigned long long)want.hps_addr);
    if (got.expect_crc != want.expect_crc)
      std::printf("      expect_crc got 0x%08X want 0x%08X\n", got.expect_crc, want.expect_crc);
    if (got.src_id != want.src_id)
      std::printf("      src_id     got 0x%08X want 0x%08X\n", got.src_id, want.src_id);
  }
  return printed + 1;
}

void reset(Vzhao_terrain_loadq& d) {
  d.rst_n = 0;
  d.j_valid_i = 0;
  d.q_ready_i = 0;
  d.drain_i = 0;
  drive_job(d, tq::LoadJob{});
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
}

// A cheap deterministic ready pattern. Pattern 0 = always, 1 = every other,
// 2 = three in four, 3 = one in eight. THE MOSTLY-READY ONE IS NOT DECORATION:
// a sibling block's differential passed a 15,625-case sweep with ready held
// high on every cycle and still missed a dropped answer, and the pattern that
// found it was three-in-four.
bool ready_draw(uint32_t& s, int pattern) {
  s = s * 1664525u + 1013904223u;
  switch (pattern) {
    case 0: return true;
    case 1: return ((s >> 16) & 1u) != 0u;
    case 2: return ((s >> 16) & 3u) != 0u;
    default: return ((s >> 16) & 7u) == 0u;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_loadq* dut = new Vzhao_terrain_loadq;
  Vzhao_terrain_loadq& d = *dut;

  std::printf("== TERRAIN.LOADQ vs zref::terrain::LoadQueue ==\n");
  std::printf("   depth %u (ruling T7's per-frame page budget), one M10K, "
              "8 words of 40 bits per job\n\n", kDepth);

  // =========================================================================
  // A -- ONE JOB, EVERY FIELD
  // =========================================================================
  // The smallest thing that can be wrong, checked first: does a single job come
  // out of the serialiser bit for bit? Eight words go in and eight come back,
  // and a block that lost word 6 would still hand over a plausible-looking job
  // with the right slot and the wrong CRC.
  {
    std::printf("-- A: one job, all nine fields --\n");
    reset(d);
    tq::LoadQueue m(kDepth);

    ck(d.j_ready_o != 0, "A the queue is ready out of reset");
    ck(d.q_valid_o == 0, "A and offers nothing");
    ck(d.inflight_o == 0, "A holding nothing", 0, (long)d.inflight_o);

    const tq::LoadJob j = job_of(0x2A);
    drive_job(d, j);
    d.j_valid_i = 1;
    d.eval();
    ck(d.j_ready_o != 0, "A it accepts the first job on the cycle it is offered");
    m.push(j);
    zhao::tick(d);
    d.j_valid_i = 0;
    d.eval();

    // The serialiser needs cycles. How many is not asserted -- that is the
    // storage decision the model deliberately does not own -- but it must
    // terminate, so the bound is generous and its exhaustion is a failure.
    int spun = 0;
    while (!d.q_valid_o && spun < 200) { zhao::tick(d); d.eval(); ++spun; }
    ck(d.q_valid_o != 0, "A and presents it to the loader within 200 cycles", 1,
       d.q_valid_o ? 1 : 0);
    std::printf("   presented after %d cycles\n", spun);

    const tq::LoadJob got = out_job(d);
    int printed = diff_report(got, j, 0, 0);
    ck(printed == 0, "A every field survives the round trip through the M10K", 0, printed);

    d.q_ready_i = 1;
    d.eval();
    zhao::tick(d);
    d.q_ready_i = 0;
    d.eval();
    m.pop();
    ck(d.q_valid_o == 0, "A and the queue is empty once it is taken");
    ck(d.accepted_o == m.accepted(), "A accepted agrees with the model",
       (long)m.accepted(), (long)d.accepted_o);
    ck(d.issued_o == m.issued(), "A issued agrees with the model", (long)m.issued(),
       (long)d.issued_o);
  }

  // =========================================================================
  // B -- FILL IT, THEN OFFER ANYWAY
  // =========================================================================
  // Capacity, and the refusal that must follow. The sequencer is TOLD by
  // `j_ready_o` and should not be offering, so the refusal counter is not a
  // normal path -- but a queue that accepted a 33rd job would overwrite a live
  // one, and the count is what proves it did not.
  {
    std::printf("\n-- B: filled to the brim, then pushed anyway --\n");
    reset(d);
    tq::LoadQueue m(kDepth);

    uint32_t next = 0;
    int guard = 0;
    while (m.held() < kHold && guard < 20000) {
      drive_job(d, job_of(next));
      d.j_valid_i = 1;
      d.eval();
      const bool took = d.j_ready_o != 0;
      if (took) { m.push(job_of(next)); ++next; }
      zhao::tick(d);
      d.eval();
      ++guard;
    }
    ck(m.held() == kHold,
       "B thirty-three jobs went in: the store's thirty-two plus the one at the port",
       (long)kHold, (long)m.held());
    std::printf("   filled in %d cycles\n", guard);

    // Now hold valid high against a full queue. Nothing may move.
    const uint32_t acc_before = d.accepted_o;
    d.j_valid_i = 1;
    drive_job(d, job_of(0xDEAD));
    d.eval();
    int cycles_full = 0;
    for (int i = 0; i < 300; ++i) {
      if (d.j_ready_o) break;
      zhao::tick(d);
      d.eval();
      ++cycles_full;
    }
    ck(cycles_full == 300,
       "B a full queue refuses for as long as it is full -- ready never blinked", 300,
       cycles_full);
    ck(d.accepted_o == acc_before,
       "B and accepted nothing while refusing", (long)acc_before, (long)d.accepted_o);
    ck(d.refused_o >= 300,
       "B the refusal counter moved: it counts CYCLES, which is the sequencer's stall "
       "and the number worth reading",
       1, d.refused_o >= 300 ? 1 : 0);
    std::printf("   refused for %u cycles while full, inflight=%u\n", d.refused_o,
                d.inflight_o);

    d.j_valid_i = 0;
    d.eval();

    // Drain it all out and compare, job for job, in order.
    int printed = 0;
    uint32_t out = 0;
    for (int i = 0; i < 40000 && m.held() > 0; ++i) {
      d.q_ready_i = 1;
      d.eval();
      if (d.q_valid_o) {
        printed = diff_report(out_job(d), m.front(), out, printed);
        m.pop();
        ++out;
      }
      zhao::tick(d);
      d.eval();
    }
    d.q_ready_i = 0;
    d.eval();
    ck(out == kHold, "B all of them came back out", (long)kHold, (long)out);
    ck(printed == 0, "B in the order they went in, field for field", 0, printed);
    ck(d.issued_o == m.issued(), "B and issued agrees with the model", (long)m.issued(),
       (long)d.issued_o);
    ck(d.high_water_o == kDepth, "B and the high water mark saw the queue full",
       (long)kDepth, (long)d.high_water_o);
  }

  // =========================================================================
  // C -- BOTH SIDES BUSY, UNDER FOUR STALL PATTERNS
  // =========================================================================
  // The pattern that finds level-counter and read-during-write faults: pushing
  // and popping on the same cycle, at every occupancy from empty to full, for
  // long enough that the store's pointers wrap several times.
  {
    std::printf("\n-- C: both sides busy, four stall patterns --\n");
    for (int pattern = 0; pattern < 4; ++pattern) {
      reset(d);
      tq::LoadQueue m(kDepth);
      uint32_t s_in = 0x1234u ^ uint32_t(pattern * 977), s_out = 0x9876u ^ uint32_t(pattern * 131);
      uint32_t next = 0, out = 0;
      int printed = 0;
      const uint32_t kJobs = 200;   // 6.25 wraps of a 32-entry store

      for (int cyc = 0; cyc < 400000 && out < kJobs; ++cyc) {
        const bool offer = (next < kJobs) && ready_draw(s_in, pattern);
        d.j_valid_i = offer ? 1 : 0;
        if (offer) drive_job(d, job_of(next));
        d.q_ready_i = ready_draw(s_out, pattern) ? 1 : 0;
        d.eval();

        const bool took = offer && d.j_ready_o;
        const bool gave = d.q_valid_o && d.q_ready_i;
        if (gave) {
          printed = diff_report(out_job(d), m.front(), out, printed);
          m.pop();
          ++out;
        }
        if (took) { m.push(job_of(next)); ++next; }

        zhao::tick(d);
        d.eval();
      }
      d.j_valid_i = 0;
      d.q_ready_i = 0;
      d.eval();

      char msg[192];
      std::snprintf(msg, sizeof msg,
                    "C pattern %d moved all %u jobs through", pattern, kJobs);
      ck(out == kJobs, msg, (long)kJobs, (long)out);
      std::snprintf(msg, sizeof msg,
                    "C pattern %d delivered them in order, field for field", pattern);
      ck(printed == 0, msg, 0, printed);
      std::snprintf(msg, sizeof msg, "C pattern %d accepted == model", pattern);
      ck(d.accepted_o == m.accepted(), msg, (long)m.accepted(), (long)d.accepted_o);
      std::snprintf(msg, sizeof msg, "C pattern %d issued == model", pattern);
      ck(d.issued_o == m.issued(), msg, (long)m.issued(), (long)d.issued_o);
      std::snprintf(msg, sizeof msg,
                    "C pattern %d conserved: accepted == issued + drained + held", pattern);
      ck(d.accepted_o == d.issued_o + d.drained_o + d.inflight_o, msg,
         (long)d.accepted_o, (long)(d.issued_o + d.drained_o + d.inflight_o));
      std::printf("   pattern %d: %u jobs, high water %u of %u\n", pattern, out,
                  d.high_water_o, kDepth);
    }
  }

  // =========================================================================
  // D -- THE DRAIN, INCLUDING THE TWO JOBS THAT ARE NOT IN THE STORE
  // =========================================================================
  // A job spends eight cycles in the write serialiser before it is committed
  // and sits in the output register after it has left the store, so up to two
  // are outside the store at any moment. A drain must take all three places and
  // COUNT all three: the composed suite's first drain check compared against
  // the store's level alone and was wrong by exactly those two.
  {
    std::printf("\n-- D: the drain --\n");
    reset(d);
    tq::LoadQueue m(kDepth);

    // Empty first. A drain against nothing is a no-op with no phantom count.
    d.drain_i = 1;
    d.eval();
    zhao::tick(d);
    d.drain_i = 0;
    d.eval();
    ck(d.drained_o == 0, "D draining an empty queue counts nothing", 0, (long)d.drained_o);
    ck(d.inflight_o == 0, "D and leaves it empty", 0, (long)d.inflight_o);

    // Now fill part way with the loader refusing, so jobs pile up, and drain
    // on a cycle when the write serialiser is mid-job.
    uint32_t next = 0;
    for (int i = 0; i < 200 && d.inflight_o < 7; ++i) {
      drive_job(d, job_of(next));
      d.j_valid_i = 1;
      d.eval();
      if (d.j_ready_o) { m.push(job_of(next)); ++next; }
      zhao::tick(d);
      d.eval();
    }
    // Leave `j_valid_i` HIGH so a job is genuinely inside the serialiser when
    // the drain lands -- that is the case the store's level cannot see.
    drive_job(d, job_of(next));
    d.j_valid_i = 1;
    d.eval();
    const uint32_t held_before = d.inflight_o;
    ck(held_before >= 4,
       "D the drain is fired against a queue really holding jobs -- draining an empty "
       "one proves the port compiles and nothing else",
       1, held_before >= 4 ? 1 : 0);

    d.drain_i = 1;
    d.eval();
    zhao::tick(d);
    d.drain_i = 0;
    d.j_valid_i = 0;
    d.eval();

    ck(d.inflight_o == 0,
       "D one pulse empties the store, the serialiser and the output register alike", 0,
       (long)d.inflight_o);
    ck(d.q_valid_o == 0, "D and nothing is offered to the loader afterwards");
    ck(d.drained_o == held_before,
       "D and every job it was holding is COUNTED -- a drain that silently empties "
       "cannot be told from a queue that was never filled",
       (long)held_before, (long)d.drained_o);
    std::printf("   drained %u jobs\n", d.drained_o);

    // AND IT STILL WORKS AFTERWARDS. A drain that left a pointer or a busy flag
    // set would make the next frame's first load vanish, and nothing in the
    // composed suite would attribute that to the drain.
    const tq::LoadJob after = job_of(0x77);
    drive_job(d, after);
    d.j_valid_i = 1;
    d.eval();
    ck(d.j_ready_o != 0, "D the queue is ready again on the cycle after a drain");
    zhao::tick(d);
    d.j_valid_i = 0;
    d.eval();
    int spun = 0;
    while (!d.q_valid_o && spun < 200) { zhao::tick(d); d.eval(); ++spun; }
    ck(d.q_valid_o != 0, "D and delivers the next job it is given", 1, d.q_valid_o ? 1 : 0);
    int printed = diff_report(out_job(d), after, 0, 0);
    ck(printed == 0, "D intact, field for field, after a drain", 0, printed);
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
