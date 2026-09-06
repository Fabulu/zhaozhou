// geom_mem_adapter_directed.cpp -- two logical geometry requesters, one ENGINE1
// client.
//
// Law: reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt section 12.
//
// WHAT THIS BLOCK IS FOR, AND THEREFORE WHAT HAS TO BE PROVED. D22 tread 10
// found that GEOM.MESHFETCH has no memory identity: the arbiter casts the SLOT
// INDEX to the client enum, so slot 3 is ENGINE1 and slot 4 is DEBUG, and the
// guard grants the asset pool to ENGINE1 alone. Sharing the one permitted
// client is the answer, and everything that can go wrong with sharing is an
// OWNERSHIP question -- whose beats are these, whose verdict was that, who is
// starved. So the checks below are about ownership, not about throughput.
//
// The two burst scales are the other half. ASSETFETCH asks 64 bytes (eight
// packed words) and MESHFETCH asks 32 (four). The brief forbids normalising the
// descriptor up to an unchecked 64-byte read, and requires any beat/last
// generator that assumed eight to be TESTED at four and eight. Both lengths run
// here, and each requester's `last` must land on its own count.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_geom_mem_adapter.h"

#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
  }
}

constexpr int kEngine1 = 3;  // ZHAO_CLIENT_ENGINE1
constexpr int kScanout = 0;  // ZHAO_CLIENT_SCANOUT -- what a leaf test might pass

struct Sim {
  Vtb_geom_mem_adapter& d;

  // The downstream memory: a guard whose verdict is a cycle late, exactly like
  // zhao_mem_guard, plus a beat generator that serves EXACTLY what the request
  // asked for -- `len >> 3` packed words -- which is what zhao_shell_top now
  // does since section 12.3's generalisation.
  //
  // MY FIRST VERSION SERVED A CONSTANT EIGHT for both lengths, as a device to
  // prove the adapter derives its own count. It proved something else: the
  // generator was still delivering A's fifth through eighth words after the
  // adapter had correctly finished A's four-word line and moved on, so every
  // later scenario started against a memory model that was mid-burst. Seventeen
  // checks failed and not one of them was the adapter -- `err_unowned` reading
  // 4 was the adapter correctly reporting my own test's stray words.
  //
  // The over-serving case is still tested, but as an explicit fault scenario
  // with a drain after it, rather than as the ambient behaviour of the memory.
  bool serving = false;
  bool ok_pending = false;
  int beat = 0;
  int expect = 8;        // packed words this line owes, from the request's len
  bool deny_next = false;
  bool over_serve = false;  // deliberately send more words than asked
  int stall_after = -1;  // insert a bubble after this beat, like a scanout burst
  int stalled = 0;

  explicit Sim(Vtb_geom_mem_adapter& dut) : d(dut) {}

  void drive() {
    d.m_ready = 0;
    d.m_ok = 0;
    d.m_violation = 0;
    d.m_beat_valid = 0;
    d.m_beat_last = 0;

    if (ok_pending) {
      ok_pending = false;
      if (deny_next) {
        d.m_violation = 1;
        deny_next = false;
      } else {
        d.m_ok = 1;
        serving = true;
        beat = 0;
      }
      return;
    }

    if (serving) {
      if (stall_after >= 0 && beat == stall_after && stalled < 3) {
        ++stalled;  // a physical burst belonging to somebody else
        return;
      }
      const int last_beat = over_serve ? (expect + 3) : (expect - 1);
      d.m_beat_valid = 1;
      d.m_beat_data = 0xA000000000000000ull | static_cast<uint64_t>(beat);
      d.m_beat_last = (beat == last_beat);
      return;
    }

    if (d.m_valid) {
      d.m_ready = 1;
      ok_pending = true;
      // The length comes from the request, exactly as the shell's return path
      // now derives it. `len` is bytes; a packed word is eight of them.
      expect = static_cast<int>(d.m_len) >> 3;
    }
  }

  void post() {
    if (serving && d.m_beat_valid) {
      const int last_beat = over_serve ? (expect + 3) : (expect - 1);
      if (beat == last_beat) {
        serving = false;
        stalled = 0;
      } else {
        ++beat;
      }
    }
  }

  void step() {
    drive();
    d.eval();
    post();
    zhao::tick(d);
  }
};

struct Seen {
  int beats = 0;
  int lasts = 0;
  int last_at = -1;
  bool ok = false;
  bool viol = false;
};

// Run `cycles`, recording what each requester saw. Requests are held valid
// until the adapter takes them, which is what a real fetcher does.
void run(Sim& s, int cycles, Seen* a, Seen* b) {
  auto& d = s.d;
  // ONE CYCLE IS drive -> eval -> OBSERVE -> advance, in that order.
  //
  // The first version observed at the TOP of the loop, before `s.step()` drove
  // the memory's inputs for that cycle, so every scenario came back exactly one
  // beat short -- eight words seen as seven, four as three. That reads like an
  // off-by-one in the adapter's count and was an off-by-one in when the bench
  // looked. Driving and observing in the same settled evaluation removes the
  // question entirely.
  for (int i = 0; i < cycles; ++i) {
    s.drive();
    d.eval();

    if (d.a_beat_valid) {
      ++a->beats;
      if (d.a_beat_last) {
        ++a->lasts;
        a->last_at = a->beats;
      }
    }
    if (d.b_beat_valid) {
      ++b->beats;
      if (d.b_beat_last) {
        ++b->lasts;
        b->last_at = b->beats;
      }
    }
    if (d.a_ok) a->ok = true;
    if (d.b_ok) b->ok = true;
    if (d.a_violation) a->viol = true;
    if (d.b_violation) b->viol = true;

    // Sample the handshake now; drop valid only AFTER the edge that latches it.
    const bool take_a = d.a_valid && d.a_ready;
    const bool take_b = d.b_valid && d.b_ready;

    s.post();
    zhao::tick(d);

    if (take_a) d.a_valid = 0;
    if (take_b) d.b_valid = 0;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_geom_mem_adapter dut;
  Sim s(dut);

  dut.rst_n = 0;
  dut.a_valid = dut.b_valid = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;

  // ---- B alone: a 64-byte payload line, eight words ------------------------
  {
    Seen a, b;
    dut.b_valid = 1;
    dut.b_addr = 0x06A0000u;
    dut.b_len = 64;
    dut.b_client = kScanout;  // a leaf test's generic client
    run(s, 200, &a, &b);

    ck(b.ok, "B's request passed the guard and B saw the OK");
    ck(b.beats == 8, "B received eight packed words for its 64-byte line", 8, b.beats);
    ck(b.last_at == 8, "and its LAST landed on word eight", 8, b.last_at);
    ck(a.beats == 0,
       "while A -- which asked for nothing -- received NO beats at all; the "
       "return is routed by the recorded owner, not broadcast",
       0, a.beats);
    ck(dut.jobs_b == 1 && dut.jobs_a == 0, "and the job is counted against B", 1,
       static_cast<long long>(dut.jobs_b));
  }

  // ---- A alone: a 32-byte descriptor, FOUR words ---------------------------
  // The OTHER burst scale, which section 12.3 asks for by name: a descriptor is
  // four packed words, and the brief forbids normalising it up to an unchecked
  // 64-byte read to make the adapter simpler. The adapter emits its own `last`
  // from its own count, so this also proves that count is per-request.
  {
    Seen a, b;
    dut.a_valid = 1;
    dut.a_addr = 0x0500000u;
    dut.a_len = 32;
    dut.a_client = kScanout;
    run(s, 200, &a, &b);

    ck(a.ok, "A's request passed the guard and A saw the OK");
    ck(a.beats == 4,
       "A received FOUR packed words for its 32-byte descriptor -- the other "
       "burst scale, which section 12.3 forbids normalising up to an unchecked "
       "64-byte read",
       4, a.beats);
    ck(a.last_at == 4,
       "and its LAST landed on word four, from the adapter's OWN count rather "
       "than from the downstream flag",
       4, a.last_at);
    ck(b.beats == 0, "and B, idle, saw none of it", 0, b.beats);
  }

  // ---- the client the guard actually sees ---------------------------------
  // Both requesters above presented SCANOUT. The guard grants the asset pool to
  // ENGINE1 alone, so a forwarded client would be refused in production and the
  // leaf test would never know.
  {
    Seen a, b;
    dut.b_valid = 1;
    dut.b_addr = 0x06A0040u;
    dut.b_len = 64;
    dut.b_client = kScanout;
    int seen_client = -1;
    // Same cycle model as run(): drive, eval, observe, advance.
    for (int i = 0; i < 200; ++i) {
      s.drive();
      dut.eval();
      if (dut.m_valid && seen_client < 0) seen_client = dut.m_client;
      const bool take_b = dut.b_valid && dut.b_ready;
      s.post();
      zhao::tick(dut);
      if (take_b) dut.b_valid = 0;
    }
    ck(seen_client == kEngine1,
       "the request reaching the guard carries ENGINE1 -- the trusted identity "
       "is SUBSTITUTED here, not forwarded from a requester that asked as "
       "SCANOUT",
       kEngine1, seen_client);
    ck(dut.m_write == 0, "and it is a READ; the asset window is read-only", 0,
       static_cast<long long>(dut.m_write));
  }

  // ---- both at once: round robin, and the loser is held not dropped --------
  {
    const uint32_t cont_before = dut.contention;
    const uint32_t ja = dut.jobs_a, jb = dut.jobs_b;
    Seen a, b;
    dut.a_valid = 1;
    dut.a_addr = 0x0500040u;
    dut.a_len = 32;
    dut.b_valid = 1;
    dut.b_addr = 0x06A0080u;
    dut.b_len = 64;
    run(s, 400, &a, &b);

    ck(dut.jobs_a == ja + 1 && dut.jobs_b == jb + 1,
       "with both asking at once, BOTH were served -- the loser of the "
       "arbitration is held, not dropped",
       1, static_cast<long long>(dut.jobs_a - ja));
    ck(a.beats == 4 && b.beats == 8,
       "and each got its own length back: A four words, B eight", 4, a.beats);
    ck(a.lasts == 1 && b.lasts == 1,
       "with exactly one LAST each -- not one shared pulse seen by both", 1,
       a.lasts);
    ck(dut.contention > cont_before,
       "and the contention was COUNTED, so the decision to allow a second "
       "outstanding request can be made against a number",
       1, static_cast<long long>(dut.contention - cont_before));
  }

  // ---- a burst belonging to somebody else, mid-line ------------------------
  // Section 12.4: "A physical scanout burst between two asset bursts must not
  // reset the logical asset record." Modelled as a gap in the returning words.
  {
    Seen a, b;
    s.stall_after = 3;
    dut.b_valid = 1;
    dut.b_addr = 0x06A00C0u;
    dut.b_len = 64;
    run(s, 400, &a, &b);
    s.stall_after = -1;

    ck(b.beats == 8,
       "a three-cycle gap mid-line -- somebody else's physical burst -- does "
       "not disturb the line: still eight words",
       8, b.beats);
    ck(b.last_at == 8, "and the LAST is still on word eight", 8, b.last_at);
    ck(a.beats == 0, "and the gap did not leak any of it to A", 0, a.beats);
  }

  // ---- a denial goes to the requester that asked --------------------------
  {
    const uint32_t den = dut.denied;
    Seen a, b;
    s.deny_next = true;
    dut.a_valid = 1;
    dut.a_addr = 0x0500080u;
    dut.a_len = 32;
    run(s, 200, &a, &b);

    ck(a.viol, "a denied request raises VIOLATION at the requester that asked");
    ck(!b.viol, "and not at the other one", 0, b.viol ? 1 : 0);
    ck(!a.ok, "the denied requester never saw an OK", 0, a.ok ? 1 : 0);
    ck(a.beats == 0, "and received no beats -- a denial returns nothing", 0, a.beats);
    ck(dut.denied == den + 1, "and it is counted", 1,
       static_cast<long long>(dut.denied - den));
  }

  // ---- nothing was lost or invented in the well-behaved scenarios ---------
  // Read BEFORE the deliberate over-serve below, because that one exists to
  // make these counters move.
  const uint32_t unowned_clean = dut.err_unowned;
  const uint32_t long_clean = dut.err_long;
  ck(unowned_clean == 0,
     "across every well-behaved scenario, no word arrived with no logical "
     "request recorded",
     0, static_cast<long long>(unowned_clean));
  ck(long_clean == 0, "and none arrived after its line was complete", 0,
     static_cast<long long>(long_clean));

  // ---- a memory that does not stop ----------------------------------------
  // Section 11.4: "late/extra data is a protocol fault". The adapter must
  // finish the useful line at its own count, report the surplus, and RETAIN the
  // physical owner through downstream LAST. A queued request is held until that
  // drain completes, so the ninth word can never become its first.
  {
    Seen a, b;
    s.over_serve = true;
    dut.b_valid = 1;
    dut.b_addr = 0x06A0100u;
    dut.b_len = 64;

    // Let B cross the adapter boundary, then queue A while B is still issuing.
    // `run` clears a valid only after the accepting edge.
    for (int i = 0; i < 20 && dut.b_valid; ++i) run(s, 1, &a, &b);
    ck(!dut.b_valid, "the overlong B fixture was accepted before A queued", 0,
       dut.b_valid ? 1 : 0);

    dut.a_valid = 1;
    dut.a_addr = 0x05000C0u;
    dut.a_len = 32;
    while (!s.serving) run(s, 1, &a, &b);
    while (s.serving) run(s, 1, &a, &b);

    ck(b.beats == 8,
       "an over-serving memory still delivers exactly EIGHT words to the "
       "requester -- the surplus is not passed on",
       8, b.beats);
    ck(b.lasts == 1, "with exactly one logical LAST", 1, b.lasts);
    ck(a.beats == 0,
       "and no surplus word leaks into the queued descriptor request", 0,
       a.beats);
    ck(dut.a_valid,
       "the queued request remains unaccepted through the physical surplus "
       "and LAST");
    ck(dut.err_unowned == unowned_clean,
       "surplus words retain B ownership while draining, so they are not "
       "misclassified as UNOWNED",
       unowned_clean, static_cast<long long>(dut.err_unowned));
    ck(dut.err_long == long_clean + 1,
       "the overlong physical response is classified exactly once as LONG", 1,
       static_cast<long long>(dut.err_long - long_clean));

    // The cycle after physical LAST may finally admit and serve queued A.
    s.over_serve = false;
    run(s, 200, &a, &b);
    ck(!dut.a_valid && a.ok,
       "the queued request is admitted after, not during, the drain");
    ck(a.beats == 4 && a.last_at == 4,
       "and it receives an uncontaminated four-word descriptor", 4, a.beats);
    ck(dut.err_short == 0,
       "with no SHORT reported anywhere in this run: nothing ended early",
       0, static_cast<long long>(dut.err_short));
  }

  std::printf(
      "  adapter: jobs A %u, jobs B %u, denied %u, contention %u; "
      "short %u long %u unowned %u\n",
      dut.jobs_a, dut.jobs_b, dut.denied, dut.contention, dut.err_short, dut.err_long,
      dut.err_unowned);

  if (g_fail) {
    std::printf("[geom_mem_adapter_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[geom_mem_adapter_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
