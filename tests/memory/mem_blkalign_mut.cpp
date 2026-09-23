// mem_blkalign_mut.cpp — THE POSITIVE CONTROL FOR THE BL8 BLOCK CLAMP, and
// its negative control, from ONE source compiled twice (owner ruling R243 /
// D-SDRAM-A).
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// R243's step 1 made `sim/models/zhao_sdram_model.sv` wrap like the part: a
// JEDEC BL8 SEQUENTIAL burst holds col[10:3] and advances col[2:0], so a burst
// started at column 5 returns 5,6,7,0,1,2,3,4. R243's step 3 then made
// `zhao_vram_arbiter.burst_words` clamp to the ALIGNED EIGHT-COLUMN BLOCK, so
// no burst it offers can ever reach that wrap.
//
// The two together are correct and they make the wrap UNREACHABLE WITH LEGAL
// STIMULUS — which is precisely the state CLAUDE.md says needs a committed
// mutant. Without one, "the model wraps now" is an argument about a line of
// SystemVerilog that nothing has ever been seen to exercise, and a repair
// nobody can watch fail is indistinguishable from a repair to nothing.
//
// ---------------------------------------------------------------------------
// THE TWO BUILDS, AND WHY BOTH HALVES ARE REQUIRED
// ---------------------------------------------------------------------------
//   mem_blkalign_mut_fires   ZHAO_BLKALIGN_MUT defined; the arbiter comes from
//                            tests/mutants/zhao_vram_arbiter_blkalign_mutant.sv,
//                            whose `burst_words` is reverted to the ROW clamp.
//                            INVERTED POLARITY: it PASSES when the misaligned
//                            write LANDS WRONG, and FAILS if memory is intact.
//   mem_blkalign_mut_silent  the same source against UNMUTATED production.
//                            DIRECT POLARITY: memory must be intact.
//
// Without the silent half, a seam that never engaged would compile two things
// that behave identically and both runs would agree. Without the fires half,
// the silent run is a counter reading zero.
//
// AND NOTE WHAT IS ASSERTED. The `silent` half asserts the CORRECT behaviour
// — every word of a misaligned request is readable at the address it was
// written to — not "the model wraps". A test that asserted the wrap would pass
// only while the defect existed and would have to be deleted by the repair.
//
// ---------------------------------------------------------------------------
// THE STIMULUS, AND WHY THESE ADDRESSES
// ---------------------------------------------------------------------------
// The SDRAM column is `addr[11:1]`, so `col mod 8` is `(addr >> 1) mod 8` and
// a 16-byte-aligned address is exactly one with `col mod 8 == 0`. CASE A uses
// an aligned address and must be intact in BOTH builds — it is the control
// that says the harness, the write path and the peek port all work, so a CASE
// B failure is about alignment and not about the plumbing. CASE B starts at
// `col mod 8 == 5`: with the block clamp the arbiter issues 3 + 8 + 8 + 8 + 5
// words and every burst stays inside its block; with the row clamp it issues
// 8 + 8 + 8 + 8, the first of which wraps and overwrites the block's own first
// five columns with the words meant for the next block.
//
// The write data is `word_data(waddr)` — a pure function of the word address,
// supplied by the harness — so the check does not need to remember anything.

#include "zhao_mem_chain.hpp"

#include <cstdio>

using namespace zhao_mem;

namespace {

int failures = 0;

void chk(bool ok, const char* what) {
  if (!ok) {
    failures++;
    std::printf("FAIL: %s\n", what);
  } else {
    std::printf("ok: %s\n", what);
  }
}

// Byte addresses inside the 64-KiB compare window. `+10` puts the column at
// 5 mod 8 (10 >> 1 == 5); `+0` is block-aligned.
constexpr uint32_t ALIGNED_ADDR   = 0x1000u;
constexpr uint32_t MISALIGNED_ADDR = 0x2000u + 10u;
constexpr unsigned REQ_LEN = 64;              // bytes -> 32 words

// Write `REQ_LEN` bytes at `addr` through client 2 and let every burst retire.
void write_request(ChainHarness& h, uint32_t addr) {
  const zref::ArbClientReq idle[5]{};
  const uint64_t g = h.drive_to_grant(2, /*write=*/true, addr, REQ_LEN, idle);
  chk(g != ~0ull, "the request was accepted at its port");
  // 32 words is at most five bursts; the worst grant-to-grant span is 18
  // cycles and a refresh can steal 13 more, so 400 idle cycles is generous
  // rather than tuned.
  h.idle_cycles(400);
}

// Every word must read back at the address it was written to. Returns the
// number of words that do not.
unsigned count_corrupt(ChainHarness& h, uint32_t addr, const char* label) {
  unsigned bad = 0;
  const uint32_t w0 = addr >> 1;
  for (unsigned i = 0; i < REQ_LEN / 2; i++) {
    const uint32_t waddr = w0 + i;
    const uint16_t got = h.model_peek(waddr);
    const uint16_t want = word_data(waddr);
    if (got != want) {
      if (bad < 8)
        std::printf("  %s: waddr=%u (col %u) got=%04x want=%04x\n", label, waddr, waddr & 0x7FFu,
                    (unsigned)got, (unsigned)want);
      bad++;
    }
  }
  std::printf("%s: %u of %u words wrong\n", label, bad, REQ_LEN / 2);
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

#ifdef ZHAO_BLKALIGN_MUT
  std::printf("mem_blkalign_mut: MUTANT build (row clamp) — INVERTED POLARITY\n");
#else
  std::printf("mem_blkalign_mut: production build — memory must be intact\n");
#endif

  ChainHarness h;
  h.reset();
  if (!h.wait_init()) {
    std::printf("mem_blkalign_mut: FAIL (init)\n");
    return 1;
  }

  // ---- CASE A: an ALIGNED request, intact in both builds ------------------
  write_request(h, ALIGNED_ADDR);
  const unsigned bad_aligned = count_corrupt(h, ALIGNED_ADDR, "aligned");
  chk(bad_aligned == 0,
      "an aligned 64-byte write lands word for word (the plumbing control)");

  // ---- CASE B: the MISALIGNED request -------------------------------------
  write_request(h, MISALIGNED_ADDR);
  const unsigned bad_misaligned = count_corrupt(h, MISALIGNED_ADDR, "misaligned");

#ifdef ZHAO_BLKALIGN_MUT
  // The mutant's first burst is 8 words from column 5 of its block, so the
  // device wraps and the last three words of that burst are written over the
  // block's own columns 0..2 instead of the next block's 0..2. At least three
  // words must therefore be wrong, and the control is worth nothing unless it
  // says WHICH way it failed.
  chk(bad_misaligned >= 3,
      "the row clamp CORRUPTS a misaligned write (the detector fires)");
  std::printf("mem_blkalign_mut: %s (mutant; %u words corrupted)\n",
              failures ? "FAIL" : "PASS", bad_misaligned);
#else
  chk(bad_misaligned == 0,
      "a misaligned 64-byte write lands word for word under the block clamp");
  std::printf("mem_blkalign_mut: %s (production; %u words corrupted)\n",
              failures ? "FAIL" : "PASS", bad_misaligned);
#endif

  // The harness also runs the zref oracles in lockstep. In the MUTANT build
  // the oracle carries the block clamp and the RTL does not, so a burst-shape
  // disagreement there is EXPECTED and is reported rather than asserted; in
  // the production build the two must agree exactly.
  std::printf("oracle mismatches: %u\n", h.mismatches);
#ifndef ZHAO_BLKALIGN_MUT
  chk(h.mismatches == 0, "RTL and the zref oracle agree cycle for cycle");
#endif

  return failures ? 1 : 0;
}
