// mem_guard_directed.cpp — MEM.GUARD directed test (plan W2.5).
//
// Contract: design/contracts/MEM.GUARD.md; law: spec/memory_rules.md §5.
// Drives the guard's muxed client port against the FULL chain (guard ->
// arbiter -> ctrl -> model), so "NOTHING was written" is proven against the
// real memory: every violation is followed by a shadow-memory compare via
// the model's peek port. Every verdict is cross-checked against
// zref::MemoryGuard.
//   * accepts blit writes inside the granted slot; scanout reads in both
//     slots
//   * rejects out-of-region writes (and reads) with NOTHING written
//   * boundary exactness: last byte in / first byte out
//   * read-only law: scanout write rejected; blit read rejected
//   * ENGINE1 16/32/64-byte reads accepted only in RENDER.ASSET_POOL;
//     client 5 and every wrong owner remain denied
//   * byte_enable holes rejected; map_valid=0 deny-all
//   * owner ruling R32: TERRAIN_BUILD WRITES the published-resource region
//     inside RENDER.ASSET_POOL, and nothing else there -- not a read, not a
//     byte outside the region, not a region straddling the pool's edge
//   * owner ruling R242: TERRAIN_BUILD READS AND WRITES TERRAIN.DEVSTORE, the
//     SDRAM home of the deviation and history records, and nothing else does;
//     both edges are exact, a burst straddling either edge is refused WHOLE,
//     and the unmapped gap between the store and TERRAIN.PAGE_POOL stays shut
//   * owner completion ruling ITEM 4 (GEOM.PARAMBUF): ENGINE1 READS either
//     view and WRITES only the view its lease NAMES; the shared scratch takes
//     both directions and only while it is ACQUIRED; all three seams refuse a
//     straddling burst WHOLE; with the lease low ENGINE1's permissions are
//     byte-for-byte what they were before the ruling, and RENDER.ASSET_POOL
//     stays READ-ONLY to it either way

#include "Vtb_zhao_mem_guard.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_mem.hpp"

#include <cstdio>
#include <initializer_list>

using namespace zref;

namespace {
int failures = 0;
void chk(bool ok, const char* what, long long expected = -1, long long actual = -1) {
  if (!ok) {
    failures++;
    std::printf("FAIL: %s (expected %lld, actual %lld)\n", what, expected, actual);
  } else {
    std::printf("ok: %s\n", what);
  }
}

struct GuardHarness {
  Vtb_zhao_mem_guard top;
  uint64_t cycle = 0;

  // write-burst tracking for wdata supply (same law as the chain harness)
  bool burst_active = false;
  uint32_t burst_addr = 0;
  unsigned burst_words = 0, burst_beat = 0;
  unsigned expect_ok = 0, expect_viol = 0;
  unsigned saw_ok = 0, saw_viol = 0;
  unsigned mismatches = 0;

  void tick() {
    // latch the accepted burst FIRST: on a hit-write the grant cycle G is
    // also the first beat-request cycle (law table), and ctrl_addr only
    // shows the accepted burst during G (the arbiter's registered offer)
    if (top.ctrl_grant) {
      burst_active = true;
      burst_addr = top.ctrl_addr;
      burst_words = top.ctrl_words == 0 ? 8u : top.ctrl_words;
      burst_beat = 0;
    }
    // supply write data for a requested beat (from the LATCHED burst:
    // during the data phase ctrl_addr may already show the next offer)
    if (top.wr_beat) {
      top.wdata = uint16_t((((burst_addr >> 1) + burst_beat) * 2654435761u) >> 13);
      if (++burst_beat >= burst_words) burst_active = false;
    }
    top.clk = 0;
    top.eval();
    // observe verdicts + violation bookkeeping DURING this cycle
    if (top.g_ok) saw_ok++;
    if (top.g_violation) saw_viol++;
    top.clk = 1;
    top.eval();
    top.clk = 0;
    top.eval();
    cycle++;
  }

  void reset() {
    top.rst_n = 0;
    top.g_valid = 0;
    top.wdata = 0;
    top.peek_en = 0;
    top.eval();
    for (int i = 0; i < 4; i++) tick();
    top.rst_n = 1;
    top.eval();
    tick();
    cycle = 0;
    saw_ok = saw_viol = 0;
    expect_ok = expect_viol = 0;
  }

  // one request through the guard (holds until ready), then drain the
  // forwarded burst; verifies the verdict against zref::MemoryGuard
  void request(const MemoryGuard::Req& r, const GuardMap& m) {
    top.map_valid = m.valid;
    top.blit_slot = m.blit_slot;
    top.blit_span = m.blit_span;
    top.fb_writer = (m.writer == GuardMap::WRITER_ENGINE0) ? 1 : 0;
    top.res_valid = m.res_valid;
    top.res_base = m.res_base;
    top.res_span = m.res_span;
    // GEOM.PARAMBUF's frame lease (owner completion ruling ITEM 4). Driven on
    // EVERY request, not only inside the PARAMARENA block, so that the ~2,000
    // fuzz requests below differential the three new arms against the oracle
    // at whatever lease state they happen to draw -- and so that every case
    // written before this ruling keeps asserting the lease-low behaviour it
    // was written against rather than silently acquiring a new permission.
    top.pb_lease_valid = m.pb_lease_valid;
    top.pb_wr_view = m.pb_wr_view;
    top.pb_scratch_valid = m.pb_scratch_valid;
    top.g_valid = 1;
    top.g_write = r.write;
    top.g_client = r.client;
    top.g_addr = r.addr;
    top.g_len = r.len;
    top.g_be = r.be;
    // expected verdict from the oracle
    const bool ok = MemoryGuard::verdict(m, r);
    if (ok)
      expect_ok++;
    else
      expect_viol++;
    const uint32_t viol0 = top.guard_violations;
    const unsigned saw_ok0 = saw_ok;
    const unsigned saw_viol0 = saw_viol;
    const uint64_t until = cycle + 5000;  // PER-REQUEST bound (cycle is
    bool accepted = false;
    while (cycle < until) {  // cumulative across requests)
      top.clk = 0;
      top.eval();
      const bool fire = top.g_valid && top.g_ready;
      tick();
      if (fire) {
        accepted = true;
        break;
      }
    }
    if (!accepted) {
      std::printf("  request was never accepted before bound\n");
      mismatches++;
    }
    // Drop valid immediately after the actual acceptance edge, before the later
    // registered verdict. A post-tick ready level is not evidence of acceptance.
    top.g_valid = 0;
    for (int i = 0; i < 120; i++) tick();
    const unsigned ok_delta = saw_ok - saw_ok0;
    const unsigned viol_delta = saw_viol - saw_viol0;
    if ((ok && (ok_delta != 1 || viol_delta != 0)) || (!ok && (ok_delta != 0 || viol_delta != 1)) ||
        top.guard_violations != viol0 + (ok ? 0u : 1u)) {
      std::printf("  request verdict multiplicity mismatch ok=%u/%u viol=%u/%u\n", ok_delta,
                  ok ? 1u : 0u, viol_delta, ok ? 0u : 1u);
      mismatches++;
    }
    static int nreq = 0;
    if (nreq++ < 6)
      std::printf("  req#%d cyc=%llu ok=%u viol=%u gviol=%u init=%d cgrant=%d\n", nreq,
                  (unsigned long long)cycle, saw_ok, saw_viol, (unsigned)top.guard_violations,
                  (int)top.init_done, (int)top.ctrl_grant);
    (void)viol0;
  }

  uint16_t peek(uint32_t waddr) {
    top.peek_en = 1;
    top.peek_waddr = waddr;
    top.clk = 0;
    top.eval();
    const uint16_t d = top.peek_data;
    top.peek_en = 0;
    top.eval();
    return d;
  }

  bool wait_init() {
    while (!top.init_done && cycle < 200) tick();
    return top.init_done != 0;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  GuardHarness h;
  h.reset();
  chk(h.wait_init(), "init completes");

  GuardMap map{true, 0, 0x0003C000};  // blit granted slot 0, full span
  auto full_be = [](unsigned len) { return len == 64 ? ~0ull : ((1ull << len) - 1); };

  // ---- accepted blit writes land in the model -----------------------------
  for (uint32_t a = 0; a < 512; a += 64) {
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, a, 64, full_be(64)}, map);
  }
  bool landed = true;
  for (uint32_t w = 0; w < 256; w++)
    landed = landed && h.peek(w) == uint16_t(((w * 2654435761u) >> 13) & 0xFFFF);
  chk(landed, "accepted blit writes visible in the model");

  // ---- out-of-region write: rejected, NOTHING written ----------------------
  {
    const uint32_t beyond = 0x0003C000;  // slot 1 territory
    for (uint32_t a = beyond; a < beyond + 256; a += 64) {
      h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, a, 64, full_be(64)}, map);
    }
    bool untouched = true;
    for (uint32_t w = 0x1E000; w < 0x1E000 + 128; w++) untouched = untouched && h.peek(w) == 0;
    chk(untouched, "out-of-region write left the shadow memory untouched");
  }

  // ---- boundary exactness ---------------------------------------------------
  {
    // last byte in: addr+len == span end
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x0003C000 - 64, 64, full_be(64)},
              map);
    chk(h.peek(0x1DFFF) == uint16_t((((0x1DFFF) * 2654435761u) >> 13) & 0xFFFF),
        "last-byte-in write lands");
    // first byte out: addr+len == span end + 1
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x0003C000 - 63, 64, full_be(64)},
              map);
    chk(h.peek(0x1E000) == 0, "first-byte-out write rejected, nothing written");
  }

  // ---- scanout law -----------------------------------------------------------
  {
    // read both slots: accepted (slot 1 sits in DRAM bank 1 since the
    // W2.7 bank split — zhao_pkg ZHAO_FB_SLOT1_BASE)
    h.request(MemoryGuard::Req{true, false, MemoryGuard::SCANOUT, 0x0000, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, false, MemoryGuard::SCANOUT, 0x02000000, 64, full_be(64)},
              map);
    // the OLD slot-1 window is now the unmapped hole: rejected
    h.request(MemoryGuard::Req{true, false, MemoryGuard::SCANOUT, 0x0003C000, 64, full_be(64)},
              map);
    // scanout read beyond slot 1: rejected
    h.request(MemoryGuard::Req{true, false, MemoryGuard::SCANOUT, 0x0203C000, 64, full_be(64)},
              map);
    // scanout WRITE: rejected (read-only law)
    h.request(MemoryGuard::Req{true, true, MemoryGuard::SCANOUT, 0x0000, 64, full_be(64)}, map);
    // blit READ: rejected by construction (Phase-2 blit never reads)
    h.request(MemoryGuard::Req{true, false, MemoryGuard::BLIT_DMA, 0x0000, 64, full_be(64)}, map);
  }

  // ---- the framebuffer-writer lease -------------------------------------------
  // Two blocks write an inactive framebuffer slot now -- DEBUG.FRAMEBLIT and
  // RASTER.FBWRITE -- and they share the SPATIAL window but not the TEMPORAL
  // permission. The lease names ONE writer; the other is refused exactly as a
  // request outside the window is. That is the whole difference between this
  // and simply granting ENGINE0 the blit's window, which would have let both
  // write the same slot in the same frame.
  {
    GuardMap eng{true, 0, 0x0003C000, GuardMap::WRITER_ENGINE0};

    // The lease holder may write, and only inside the window.
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)}, eng);
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, 0x0003BFC0, 64, full_be(64)}, eng);
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, 0x0003C000, 64, full_be(64)}, eng);

    // OWNER MISMATCH, BOTH WAYS. The engine holds the lease, so the blit is
    // refused; the blit holds it, so the engine is refused. Neither is an
    // address error -- both requests are squarely inside the window, which is
    // exactly why a window check alone would have passed them.
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x0000, 64, full_be(64)}, eng);
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)}, map);

    // THE POST.COMPOSITE LEASE (2026-09-19): the engine that holds the lease
    // may READ its own window -- "an exclusive framebuffer read/write lease
    // after resolve and before publication". It used to read nothing. The
    // polarity is asserted against the oracle HERE, not only differenced,
    // because a request the RTL and the oracle both wrongly refuse agrees.
    auto expect_verdict = [&](const MemoryGuard::Req& r, const GuardMap& m, bool want,
                              const char* what) {
      if (MemoryGuard::verdict(m, r) != want) {
        std::printf("  ORACLE POLARITY: %s expected %s\n", what, want ? "PASS" : "DENY");
        h.mismatches++;
      }
      h.request(r, m);
    };
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)},
                   eng, true, "ENGINE0 read inside its lease");
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x0003BFC0, 64, full_be(64)},
                   eng, true, "ENGINE0 read at the window's last line");
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x0003C000, 64, full_be(64)},
                   eng, false, "ENGINE0 read past the window");
    // ...and only while it HOLDS the lease: the blit's lease refuses it.
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)},
                   map, false, "ENGINE0 read under the blit's lease");
    // The OTHER slot is not the lease, whoever asks.
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x02000000, 64, full_be(64)},
                   eng, false, "ENGINE0 read of the unleased slot");
    GuardMap none{false, 0, 0x0003C000, GuardMap::WRITER_ENGINE0};
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)}, none);
    expect_verdict(MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, 0x0000, 64, full_be(64)},
                   none, false, "ENGINE0 read with no lease");

    // POST.ECHO's CAPTURE (ruling R7, spec/memory_rules.md 5g): ENGINE0,
    // WRITE-only, constant bounds, lease-gated. Both ends, one past, the read
    // direction, the blit's lease, and every other client.
    expect_verdict(
        MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, kPostEchoBase, 32, full_be(32)}, eng,
        true, "echo write at the capture base");
    expect_verdict(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0,
                                    kPostEchoBase + kPostEchoSpan - 32, 32, full_be(32)},
                   eng, true, "echo write at the capture's last line");
    expect_verdict(MemoryGuard::Req{true, true, MemoryGuard::ENGINE0,
                                    kPostEchoBase + kPostEchoSpan - 31, 32, full_be(32)},
                   eng, false, "echo write one byte past the capture");
    expect_verdict(
        MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, kPostEchoBase - 8, 32, full_be(32)}, eng,
        false, "echo write below the capture");
    expect_verdict(
        MemoryGuard::Req{true, false, MemoryGuard::ENGINE0, kPostEchoBase, 32, full_be(32)}, eng,
        false, "a READ of the capture");
    expect_verdict(
        MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, kPostEchoBase, 32, full_be(32)}, map,
        false, "echo write under the blit's lease");
    // Not frame-scoped: the capture is a constant window, so no map is needed.
    expect_verdict(
        MemoryGuard::Req{true, true, MemoryGuard::ENGINE0, kPostEchoBase, 32, full_be(32)}, none,
        true, "echo write with the lease named but no slot window");
    for (unsigned c : {unsigned(MemoryGuard::SCANOUT), unsigned(MemoryGuard::BLIT_DMA),
                       unsigned(MemoryGuard::ENGINE1), unsigned(MemoryGuard::DEBUG), 5u,
                       unsigned(MemoryGuard::TERRAIN_BUILD)}) {
      expect_verdict(MemoryGuard::Req{true, true, c, kPostEchoBase, 32, full_be(32)}, eng, false,
                     "a capture write from a client that is not ENGINE0");
    }

    // ENGINE1 owns nothing in framebuffer space; DEBUG owns no region.
    for (unsigned c = MemoryGuard::ENGINE1; c <= MemoryGuard::DEBUG; c++) {
      h.request(MemoryGuard::Req{true, false, c, 0x0000, 64, full_be(64)}, map);
      h.request(MemoryGuard::Req{true, true, c, 0x0000, 64, full_be(64)}, map);
      h.request(MemoryGuard::Req{true, true, c, 0x0000, 64, full_be(64)}, eng);
    }
  }

  // ---- shared RENDER.ASSET_POOL: one read-only ENGINE1 window ------------------
  {
    constexpr uint32_t base = kRenderAssetBase;
    constexpr uint32_t end = kRenderAssetBase + kRenderAssetSpan;
    for (unsigned len : {16u, 32u, 64u}) {
      h.request(MemoryGuard::Req{true, false, MemoryGuard::ENGINE1, base, len, full_be(len)}, map);
      h.request(MemoryGuard::Req{true, false, MemoryGuard::ENGINE1, end - len, len, full_be(len)},
                map);
      h.request(
          MemoryGuard::Req{true, false, MemoryGuard::ENGINE1, end - len + 1, len, full_be(len)},
          map);
    }
    h.request(MemoryGuard::Req{true, false, MemoryGuard::ENGINE1, base - 1, 16, full_be(16)}, map);
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE1, base, 16, full_be(16)}, map);
    const unsigned wrong_clients[] = {MemoryGuard::SCANOUT,
                                      MemoryGuard::BLIT_DMA,
                                      MemoryGuard::ENGINE0,
                                      MemoryGuard::DEBUG,
                                      5u,
                                      MemoryGuard::TERRAIN_BUILD};
    for (unsigned wrong_client : wrong_clients) {
      h.request(MemoryGuard::Req{true, false, wrong_client, base, 16, full_be(16)}, map);
    }
  }

  // ---- R32: the published-resource WRITE arm ----------------------------------
  // The top 4 KiB of RENDER.ASSET_POOL, where the console smoke lands its
  // MATERIAL_SET. Writes inside land in the model; everything that is not a
  // write inside a pool-contained region is refused with NOTHING written.
  {
    constexpr uint32_t rb = kRenderAssetBase + kRenderAssetSpan - 0x1000u;  // 0x07FF_F000
    GuardMap res = map;
    res.res_valid = true;
    res.res_base = rb;
    res.res_span = 0x1000u;
    const unsigned TB = MemoryGuard::TERRAIN_BUILD;
    auto pat = [](uint32_t w) { return uint16_t(((w * 2654435761u) >> 13) & 0xFFFF); };

    h.request(MemoryGuard::Req{true, true, TB, rb, 64, full_be(64)}, res);
    h.request(MemoryGuard::Req{true, true, TB, rb + 0x1000u - 64, 64, full_be(64)}, res);
    chk(h.peek(rb >> 1) == pat(rb >> 1) && h.peek((rb + 0xFFEu) >> 1) == pat((rb + 0xFFEu) >> 1),
        "R32: writes at both ends of the resource region land");
    // one byte past the region's end, and one region-width below it (still
    // INSIDE the pool): both refused, nothing written
    h.request(MemoryGuard::Req{true, true, TB, rb + 0x1000u - 63, 64, full_be(64)}, res);
    h.request(MemoryGuard::Req{true, true, TB, rb - 64, 64, full_be(64)}, res);
    chk(h.peek((rb - 64) >> 1) == 0,
        "R32: a pool write outside the region leaves memory untouched");
    // write-only: TERRAIN_BUILD may not READ the asset pool through this arm
    h.request(MemoryGuard::Req{true, false, TB, rb, 64, full_be(64)}, res);
    // other clients gain nothing from the region
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE1, rb, 64, full_be(64)}, res);
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, rb, 64, full_be(64)}, res);
    // the region switched off
    GuardMap off = res;
    off.res_valid = false;
    h.request(MemoryGuard::Req{true, true, TB, rb, 64, full_be(64)}, off);
    // regions that are NOT wholly inside the pool close the arm whole: past its
    // end, below its start, and a base+span that wraps 32 bits
    GuardMap hi = res;
    hi.res_span = 0x2000u;
    h.request(MemoryGuard::Req{true, true, TB, rb, 64, full_be(64)}, hi);
    GuardMap lo = res;
    lo.res_base = kRenderAssetBase - 0x1000u;
    lo.res_span = 0x2000u;
    h.request(MemoryGuard::Req{true, true, TB, kRenderAssetBase, 64, full_be(64)}, lo);
    chk(h.peek(kRenderAssetBase >> 1) == 0,
        "R32: a region straddling the pool start writes nothing");
    GuardMap wrap = res;
    wrap.res_base = 0xFFFFF000u;
    wrap.res_span = 0x2000u;
    h.request(MemoryGuard::Req{true, true, TB, 0x00000000u, 64, full_be(64)}, wrap);
    chk(h.peek(0) == pat(0),
        "R32: a wrapping region leaves framebuffer slot 0 as the blit wrote it");
    // and a region inside TERRAIN.PAGE_POOL adds nothing to that arm's verdicts
    GuardMap pp = res;
    pp.res_base = kTerrainPagePoolBase;
    pp.res_span = 0x1000u;
    h.request(MemoryGuard::Req{true, false, TB, kTerrainPagePoolBase, 64, full_be(64)}, pp);
  }

  // ---- R242: TERRAIN.DEVSTORE, both directions ---------------------------------
  // The owner moved the 185 M10K deviation/history store into SDRAM
  // (2026-09-22). This is the window that made it possible, and it is tested
  // the way the page pool's arms are: both directions land, every edge is
  // exact, every other client is refused, and a request that STRADDLES an edge
  // is refused whole rather than clamped into something legal-looking.
  {
    constexpr uint32_t db = kTerrainDevStoreBase;
    constexpr uint32_t de = kTerrainDevStoreBase + kTerrainDevStoreSpan;  // 0x058B_0000
    const unsigned TB = MemoryGuard::TERRAIN_BUILD;
    const GuardMap dev_eng{true, 0, 0x0003C000, GuardMap::WRITER_ENGINE0};
    auto pat = [](uint32_t w) { return uint16_t(((w * 2654435761u) >> 13) & 0xFFFF); };

    // the block's own two sub-pools, at their first and last burst: the
    // deviations at db, the history row of slot 1,023 at de-64.
    h.request(MemoryGuard::Req{true, true, TB, db, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, true, TB, de - 64, 64, full_be(64)}, map);
    chk(h.peek(db >> 1) == pat(db >> 1) && h.peek((de - 64) >> 1) == pat((de - 64) >> 1),
        "R242: writes at both ends of TERRAIN.DEVSTORE land");
    h.request(MemoryGuard::Req{true, false, TB, db, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, false, TB, de - 64, 64, full_be(64)}, map);

    // THE EDGES ARE EXACT AND A STRADDLE IS REFUSED WHOLE. One byte past the
    // top; one burst below the base; and the two requests that lie half in and
    // half out, which are the cases the packet names -- both endpoints being
    // somewhere in the union of permitted ranges is not permission.
    h.request(MemoryGuard::Req{true, true, TB, de - 63, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, false, TB, de - 63, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, true, TB, db - 64, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, true, TB, db - 32, 64, full_be(64)}, map);
    h.request(MemoryGuard::Req{true, false, TB, db - 32, 64, full_be(64)}, map);
    chk(h.peek((db - 64) >> 1) == 0 && h.peek((db - 32) >> 1) == 0,
        "R242: a write straddling the store's lower edge leaves memory untouched");
    // de-32 is INSIDE the legal last burst written above, so the word that
    // separates "refused" from "clamped" here is the one just ABOVE the store.
    chk(h.peek(de >> 1) == 0,
        "R242: a write straddling the store's upper edge leaves memory untouched");

    // THE STORE IS NOT THE PAGE POOL. A burst at the page pool's own top edge
    // is refused by the pool's arm and gains nothing from the store's, and the
    // gap between them (0x054E_0000..0x0586_0000) is unmapped for everybody.
    h.request(MemoryGuard::Req{true, true, TB, kTerrainPagePoolBase + kTerrainPagePoolSpan - 32, 64,
                               full_be(64)},
              map);
    h.request(MemoryGuard::Req{true, true, TB, kTerrainPagePoolBase + kTerrainPagePoolSpan, 64,
                               full_be(64)},
              map);
    h.request(MemoryGuard::Req{true, false, TB, db - 0x10000u, 64, full_be(64)}, map);
    chk(h.peek((kTerrainPagePoolBase + kTerrainPagePoolSpan) >> 1) == 0,
        "R242: the gap between the page pool and the store is unmapped");

    // NO OTHER CLIENT REACHES IT, IN EITHER DIRECTION. Client 5 is included
    // because it is the unspent reservation and this window did not spend it.
    for (unsigned c : {unsigned(MemoryGuard::SCANOUT), unsigned(MemoryGuard::BLIT_DMA),
                       unsigned(MemoryGuard::ENGINE0), unsigned(MemoryGuard::ENGINE1),
                       unsigned(MemoryGuard::DEBUG), 5u}) {
      h.request(MemoryGuard::Req{true, true, c, db, 64, full_be(64)}, map);
      h.request(MemoryGuard::Req{true, false, c, db, 64, full_be(64)}, map);
      // and under the OTHER framebuffer lease, so ENGINE0 is refused whether
      // or not it holds it -- the store is not a lease-gated window.
      h.request(MemoryGuard::Req{true, true, c, db, 64, full_be(64)}, dev_eng);
    }
    chk(h.peek((db + 64) >> 1) == 0, "R242: no other client writes the store");

    // R32's resource region cannot be pointed at the store to widen it: the
    // region must lie wholly inside RENDER.ASSET_POOL, and this one does not,
    // so the arm is closed and ENGINE1 still has no write anywhere.
    GuardMap over = map;
    over.res_valid = true;
    over.res_base = db;
    over.res_span = 0x1000u;
    h.request(MemoryGuard::Req{true, true, MemoryGuard::ENGINE1, db + 128, 64, full_be(64)}, over);
    chk(h.peek((db + 128) >> 1) == 0,
        "R242: a resource region aimed at the store opens nothing for ENGINE1");
  }

  // ---- ITEM 4: GEOM.PARAMBUF, the leased geometry arena -------------------------
  // The owner's completion ruling of 2026-09-22 opened a THREE-REGION window for
  // ENGINE1 -- two disjoint views and a shared scratch -- and the three arms it
  // needs are the first in this block that are gated on something other than an
  // address. So this section is organised around the two things an address-only
  // test cannot see: WHICH view the lease names, and WHETHER the scratch was
  // acquired.
  //
  // ORDERING IS LOAD-BEARING HERE AND IS NOT AN ACCIDENT. The three regions TILE,
  // so a seam-straddling burst overlaps the legal last burst of the region below
  // it and the legal first burst of the region above it. "Nothing was written" can
  // therefore only be proven against words that are still pristine -- which means
  // every REFUSAL and its shadow-memory evidence runs BEFORE any legal write lands
  // in the arena. Reads first (they move no memory), then refusals, then the
  // writes that must land. Re-order these and the evidence quietly starts
  // measuring the previous case's legal write instead of this case's refusal,
  // which would still pass and would prove nothing.
  {
    constexpr uint32_t V0 = kParamBufView0Base;                        // 0x0600_0000
    constexpr uint32_t V0E = kParamBufView0Base + kParamBufViewSpan;   // 0x0640_0000
    constexpr uint32_t V1 = kParamBufView1Base;                        // 0x0640_0000
    constexpr uint32_t V1E = kParamBufView1Base + kParamBufViewSpan;   // 0x0680_0000
    constexpr uint32_t SC = kParamBufScratchBase;                      // 0x0680_0000
    constexpr uint32_t SCE = kParamBufScratchBase + kParamBufScratchSpan;  // 0x06A0_0000
    static_assert(V0E == V1 && V1E == SC && SCE == kRenderAssetBase,
                  "the three PARAMBUF regions must tile and end at RENDER.ASSET_POOL");
    const unsigned E1 = MemoryGuard::ENGINE1;
    auto pat = [](uint32_t w) { return uint16_t(((w * 2654435761u) >> 13) & 0xFFFF); };

    // The lease states, built from the ordinary blit map so that NOTHING about
    // the framebuffer lease is being varied at the same time. The PARAMBUF arms
    // read neither `map_valid` nor `fb_writer`, and leaving those alone is what
    // makes a failure here attributable to the arm under test.
    GuardMap pb0 = map;                                 // lease held, view 0 writable
    pb0.pb_lease_valid = true;
    GuardMap pb1 = pb0;                                 // lease held, view 1 writable
    pb1.pb_wr_view = 1;
    GuardMap pb0s = pb0;                                // ...and the scratch acquired
    pb0s.pb_scratch_valid = true;
    GuardMap pb1s = pb1;
    pb1s.pb_scratch_valid = true;
    GuardMap nolease = map;                             // the pre-ruling ENGINE1
    GuardMap scr_only = map;                            // acquire with no lease
    scr_only.pb_scratch_valid = true;

    // Polarity is ASSERTED against the oracle, not merely differenced. A case
    // the RTL and the oracle both wrongly refuse agrees perfectly, and an arm
    // that never passes anything is the cheapest way to satisfy a refusal suite.
    auto want = [&](const MemoryGuard::Req& r, const GuardMap& m, bool pass, const char* what) {
      if (MemoryGuard::verdict(m, r) != pass) {
        std::printf("  ORACLE POLARITY: %s expected %s\n", what, pass ? "PASS" : "DENY");
        h.mismatches++;
      }
      h.request(r, m);
    };

    // -- PHASE 1: READS TAKE EITHER VIEW, AND THE WRITE SELECTOR DOES NOT GATE THEM
    // The walker reads the PUBLISHED view while the producer builds the other, so
    // a read arm that honoured `pb_wr_view` would refuse exactly the traffic the
    // window exists for. Both views are therefore read under BOTH selector
    // polarities: four passes where a copy-pasted `pb_wr_view ? ... : ...` in the
    // read arm would give two.
    for (const GuardMap* m : {&pb0, &pb1}) {
      want(MemoryGuard::Req{true, false, E1, V0, 64, full_be(64)}, *m, true,
           "ENGINE1 read at view 0's base");
      want(MemoryGuard::Req{true, false, E1, V0E - 64, 64, full_be(64)}, *m, true,
           "ENGINE1 read at view 0's last burst");
      want(MemoryGuard::Req{true, false, E1, V1, 64, full_be(64)}, *m, true,
           "ENGINE1 read at view 1's base");
      want(MemoryGuard::Req{true, false, E1, V1E - 64, 64, full_be(64)}, *m, true,
           "ENGINE1 read at view 1's last burst");
    }

    // -- PHASE 2a: THE WRITE GOES TO THE VIEW THE LEASE NAMES, AND ONLY THAT ONE
    // THE HEADLINE PROTECTION. Both of these writes are squarely contained in a
    // PARAMBUF view, under a valid lease, from the right client -- so every
    // address-shaped check in the block passes them. What refuses them is the mux
    // on `pb_wr_view`, and nothing else does. This is the producer being stopped
    // from scribbling on the frame the renderer is walking, which is the entire
    // reason there are two views rather than one.
    want(MemoryGuard::Req{true, true, E1, V1 + 0x1000u, 64, full_be(64)}, pb0, false,
         "ENGINE1 write into view 1 while the lease names view 0");
    want(MemoryGuard::Req{true, true, E1, V0 + 0x1000u, 64, full_be(64)}, pb1, false,
         "ENGINE1 write into view 0 while the lease names view 1");
    chk(h.peek((V1 + 0x1000u) >> 1) == 0 && h.peek((V0 + 0x1000u) >> 1) == 0,
        "ITEM 4: a write to the view the lease does not name leaves memory untouched");

    // -- PHASE 2b: EVERY SEAM REFUSES A STRADDLING BURST, WHOLE
    // Item 4: "A request crossing a per-view or scratch boundary is not allowed
    // merely because both endpoints lie somewhere in the union of permitted
    // ranges." These three bursts are exactly that request. Each starts 32 bytes
    // below a seam and ends 32 bytes above it, so every byte it touches is inside
    // SOME permitted region -- and a single [VIEW0_BASE, SCRATCH_END) comparison
    // would admit all three while being indistinguishable from the correct
    // arrangement on every request that does not cross a seam. Both directions,
    // because the read arm's `in_view0 || in_view1` is the one place a union could
    // be reintroduced without anyone noticing.
    want(MemoryGuard::Req{true, true, E1, V0E - 32, 64, full_be(64)}, pb0, false,
         "a WRITE straddling the view0/view1 seam");
    want(MemoryGuard::Req{true, true, E1, V0E - 32, 64, full_be(64)}, pb1, false,
         "a WRITE straddling the view0/view1 seam, other selector");
    want(MemoryGuard::Req{true, false, E1, V0E - 32, 64, full_be(64)}, pb0, false,
         "a READ straddling the view0/view1 seam");
    want(MemoryGuard::Req{true, true, E1, V1E - 32, 64, full_be(64)}, pb1s, false,
         "a WRITE straddling the view1/scratch seam");
    want(MemoryGuard::Req{true, false, E1, V1E - 32, 64, full_be(64)}, pb0s, false,
         "a READ straddling the view1/scratch seam");
    want(MemoryGuard::Req{true, true, E1, SCE - 32, 64, full_be(64)}, pb0s, false,
         "a WRITE straddling the scratch/asset-pool seam");
    // The scratch/asset read is the seam worth naming twice: the scratch is
    // readable and the asset pool is readable, so their UNION would pass this
    // request, and the only thing refusing it is that neither region contains it
    // whole. It is also the request that would silently widen ENGINE1's asset-pool
    // access by half a burst.
    want(MemoryGuard::Req{true, false, E1, SCE - 32, 64, full_be(64)}, pb0s, false,
         "a READ straddling the scratch/asset-pool seam");
    // A 16-byte straddle of the view1/scratch seam, because a 64-byte burst is the
    // only size the cases above use and the containment arithmetic is length-
    // sensitive at exactly one place (`end`).
    want(MemoryGuard::Req{true, true, E1, SC - 1, 16, full_be(16)}, pb0s, false,
         "a short WRITE straddling the view1/scratch seam");
    chk(h.peek(V0E >> 1) == 0 && h.peek((V0E - 32) >> 1) == 0,
        "ITEM 4: a burst straddling the view0/view1 seam writes nothing on either side");
    chk(h.peek(V1E >> 1) == 0 && h.peek((V1E - 32) >> 1) == 0,
        "ITEM 4: a burst straddling the view1/scratch seam writes nothing on either side");
    chk(h.peek(SCE >> 1) == 0 && h.peek((SCE - 32) >> 1) == 0,
        "ITEM 4: a burst straddling the scratch/asset-pool seam writes nothing on either side");

    // -- PHASE 2c: THE EDGES ARE EXACT, ONE BYTE EITHER WAY
    // The refused halves live here; the passing halves are in phase 3, where they
    // can be shown to LAND. A region whose top bound was written `<` instead of
    // `<=`, or whose base used `>`, passes every case above and fails exactly one
    // of these four.
    want(MemoryGuard::Req{true, true, E1, V0E - 63, 64, full_be(64)}, pb0, false,
         "a view-0 write one byte past the view's end");
    want(MemoryGuard::Req{true, false, E1, V0 - 1, 16, full_be(16)}, pb0, false,
         "a read one byte below view 0's base");
    want(MemoryGuard::Req{true, true, E1, SCE - 63, 64, full_be(64)}, pb0s, false,
         "a scratch write one byte past the scratch's end");
    want(MemoryGuard::Req{true, false, E1, SCE - 63, 64, full_be(64)}, pb0s, false,
         "a scratch read one byte past the scratch's end");

    // -- PHASE 2d: THE SCRATCH IS UNMAPPED UNTIL IT IS ACQUIRED
    // Item 4: "Shared scratch has explicit ownership and release rather than being
    // unowned temporary memory." Release is deasserting `pb_scratch_valid`, so a
    // released scratch must be as closed as an address outside the map -- in BOTH
    // directions, because the fetcher writes it and the walker reads it and a
    // direction-gated arm would leave one of them open after release.
    want(MemoryGuard::Req{true, true, E1, SC + 0x1000u, 64, full_be(64)}, pb0, false,
         "a scratch WRITE with the scratch not acquired");
    want(MemoryGuard::Req{true, false, E1, SC + 0x1000u, 64, full_be(64)}, pb1, false,
         "a scratch READ with the scratch not acquired");
    chk(h.peek((SC + 0x1000u) >> 1) == 0,
        "ITEM 4: a write to an unacquired scratch leaves memory untouched");

    // -- PHASE 2e: NO BLANKET BANK-3 PERMISSION
    // With the lease low, ENGINE1's permissions must be BYTE-FOR-BYTE what they
    // were before this ruling. That is the property the whole three-port design
    // exists to make checkable, and it is checked positively AND negatively here:
    // everything 5c opened is shut, and the one thing ENGINE1 already had --
    // reading RENDER.ASSET_POOL -- still works. A regression that tied the lease
    // high internally would pass every other case in this section.
    want(MemoryGuard::Req{true, false, E1, V0, 64, full_be(64)}, nolease, false,
         "a view-0 read with no lease");
    want(MemoryGuard::Req{true, false, E1, V1, 64, full_be(64)}, nolease, false,
         "a view-1 read with no lease");
    want(MemoryGuard::Req{true, true, E1, V0 + 0x2000u, 64, full_be(64)}, nolease, false,
         "a view-0 write with no lease");
    want(MemoryGuard::Req{true, true, E1, V1 + 0x2000u, 64, full_be(64)}, nolease, false,
         "a view-1 write with no lease");
    // An acquired scratch is not a lease. `pb_scratch_valid` alone opens nothing:
    // the scratch arm carries BOTH terms, so releasing the frame closes the
    // scratch even if nobody remembered to deassert its own bit.
    want(MemoryGuard::Req{true, true, E1, SC + 0x2000u, 64, full_be(64)}, scr_only, false,
         "a scratch write with the scratch acquired but no lease");
    want(MemoryGuard::Req{true, false, E1, SC + 0x2000u, 64, full_be(64)}, scr_only, false,
         "a scratch read with the scratch acquired but no lease");
    want(MemoryGuard::Req{true, false, E1, kRenderAssetBase, 64, full_be(64)}, nolease, true,
         "the pre-ruling asset-pool read, with no lease");
    chk(h.peek((V0 + 0x2000u) >> 1) == 0 && h.peek((V1 + 0x2000u) >> 1) == 0 &&
            h.peek((SC + 0x2000u) >> 1) == 0,
        "ITEM 4: with the lease low, nothing in the arena is writable");

    // -- PHASE 2f: RENDER.ASSET_POOL IS STILL READ-ONLY TO ENGINE1
    // Named explicitly by the owner. The pool sits immediately above the scratch,
    // so the arrangement that would break this is not exotic: one span constant
    // off by 0x0020_0000, or the scratch arm written over the union of its own
    // region and the pool. Refused with the lease held (both selectors, scratch
    // acquired) and with it not held, because "the lease bought a write" and "bank
    // 3 was always writable" are two different defects and this distinguishes them.
    want(MemoryGuard::Req{true, true, E1, kRenderAssetBase, 64, full_be(64)}, pb0s, false,
         "an ENGINE1 WRITE into RENDER.ASSET_POOL with the lease held, view 0");
    want(MemoryGuard::Req{true, true, E1, kRenderAssetBase, 64, full_be(64)}, pb1s, false,
         "an ENGINE1 WRITE into RENDER.ASSET_POOL with the lease held, view 1");
    want(MemoryGuard::Req{true, true, E1, kRenderAssetBase, 64, full_be(64)}, nolease, false,
         "an ENGINE1 WRITE into RENDER.ASSET_POOL with no lease");
    want(MemoryGuard::Req{true, false, E1, kRenderAssetBase, 64, full_be(64)}, pb0s, true,
         "the asset-pool READ, unchanged by the ruling");
    chk(h.peek(kRenderAssetBase >> 1) == 0,
        "ITEM 4: ENGINE1 cannot write RENDER.ASSET_POOL, leased or not");

    // -- PHASE 2g: ONE CLIENT
    // Item 4: "No other client acquires PARAMBUF access through this ruling. NO
    // blanket bank-3 permission." Every other client id, both directions, all
    // three regions, under the MOST permissive lease state there is -- so a
    // verdict that leaked out of the ENGINE1 case arm has nowhere to hide. Client
    // 5 is included because it is ruling T3's unspent reservation and this window
    // did not spend it; TERRAIN_BUILD because its four arms are evaluated over
    // TERRAIN's constants and must admit nothing here.
    for (unsigned c : {unsigned(MemoryGuard::SCANOUT), unsigned(MemoryGuard::BLIT_DMA),
                       unsigned(MemoryGuard::ENGINE0), unsigned(MemoryGuard::DEBUG), 5u,
                       unsigned(MemoryGuard::TERRAIN_BUILD)}) {
      for (uint32_t base : {V0, V1, SC}) {
        want(MemoryGuard::Req{true, true, c, base + 0x3000u, 64, full_be(64)}, pb0s, false,
             "a PARAMBUF write from a client that is not ENGINE1");
        want(MemoryGuard::Req{true, false, c, base + 0x3000u, 64, full_be(64)}, pb0s, false,
             "a PARAMBUF read from a client that is not ENGINE1");
      }
    }
    chk(h.peek((V0 + 0x3000u) >> 1) == 0 && h.peek((V1 + 0x3000u) >> 1) == 0 &&
            h.peek((SC + 0x3000u) >> 1) == 0,
        "ITEM 4: no client but ENGINE1 writes the arena");

    // -- PHASE 2h: shape_ok STILL GATES
    // A malformed request inside a perfectly legal range. `pass_ok` is
    // `shape_ok && (...)` for ENGINE1 exactly as it is for every other client, and
    // the new arms are on the right-hand side of that AND -- this is the case that
    // says so, rather than leaving it to be read off the case statement.
    want(MemoryGuard::Req{true, true, E1, V0 + 0x4000u, 64, full_be(64) & ~0xFull}, pb0, false,
         "a legally addressed view-0 write with a hole in the byte mask");
    want(MemoryGuard::Req{true, false, E1, V1 + 0x4000u, 32, full_be(16)}, pb1, false,
         "a legally addressed view-1 read whose mask is the wrong width for its len");
    want(MemoryGuard::Req{true, true, E1, SC + 0x4000u, 65, full_be(64)}, pb0s, false,
         "a legally addressed scratch write of an illegal length");
    chk(h.peek((V0 + 0x4000u) >> 1) == 0 && h.peek((SC + 0x4000u) >> 1) == 0,
        "ITEM 4: a shape violation inside a legal range writes nothing");

    // -- PHASE 3: THE LEGAL WRITES, AND THEY LAND
    // Everything above is a refusal, and a window that refuses everything refuses
    // all of them. These are the passes, proven against the real memory through
    // the model's peek port: both ends of the leased view under each selector
    // polarity, and both ends of the acquired scratch. The view-0 burst at V0E-64
    // ends EXACTLY at 0x0640_0000 and is the passing half of phase 2c's first
    // case; the scratch burst at SCE-64 ends exactly at RENDER.ASSET_POOL's base
    // and is the passing half of its third.
    want(MemoryGuard::Req{true, true, E1, V0, 64, full_be(64)}, pb0, true,
         "ENGINE1 write at view 0's base under view 0's lease");
    want(MemoryGuard::Req{true, true, E1, V0E - 64, 64, full_be(64)}, pb0, true,
         "ENGINE1 write ending EXACTLY at view 0's top edge");
    want(MemoryGuard::Req{true, true, E1, V1, 64, full_be(64)}, pb1, true,
         "ENGINE1 write at view 1's base under view 1's lease");
    want(MemoryGuard::Req{true, true, E1, V1E - 64, 64, full_be(64)}, pb1, true,
         "ENGINE1 write ending EXACTLY at view 1's top edge");
    chk(h.peek(V0 >> 1) == pat(V0 >> 1) && h.peek((V0E - 64) >> 1) == pat((V0E - 64) >> 1) &&
            h.peek(V1 >> 1) == pat(V1 >> 1) && h.peek((V1E - 64) >> 1) == pat((V1E - 64) >> 1),
        "ITEM 4: writes at both ends of each leased view land");
    // The scratch takes both directions and ignores `pb_wr_view` -- it is ONE
    // region with one owner, not a third view -- so its write is exercised under
    // both selector polarities and must pass under each.
    want(MemoryGuard::Req{true, true, E1, SC, 64, full_be(64)}, pb0s, true,
         "scratch write with the scratch acquired, selector 0");
    want(MemoryGuard::Req{true, true, E1, SCE - 64, 64, full_be(64)}, pb1s, true,
         "scratch write ending EXACTLY at RENDER.ASSET_POOL's base, selector 1");
    want(MemoryGuard::Req{true, false, E1, SC, 64, full_be(64)}, pb0s, true,
         "scratch read with the scratch acquired");
    want(MemoryGuard::Req{true, false, E1, SCE - 64, 64, full_be(64)}, pb1s, true,
         "scratch read at the scratch's last burst");
    chk(h.peek(SC >> 1) == pat(SC >> 1) && h.peek((SCE - 64) >> 1) == pat((SCE - 64) >> 1),
        "ITEM 4: writes at both ends of the acquired scratch land");
  }

  // ---- byte_enable holes rejected ---------------------------------------------
  {
    h.request(
        MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x1000, 64, full_be(64) & ~0xFull},
        map);  // hole in the mask
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x1000, 0, 0},
              map);  // len 0
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x1000, 65, full_be(64)},
              map);  // len 65
  }

  // ---- deny-all when map_valid = 0 --------------------------------------------
  {
    GuardMap nomap{false, 0, 0x0003C000};
    h.request(MemoryGuard::Req{true, true, MemoryGuard::BLIT_DMA, 0x0000, 64, full_be(64)}, nomap);
  }

  // ---- PCG boundary fuzz: every verdict == oracle ------------------------------
  {
    zref::Pcg32 pcg(0x5EEDF00Du);
    // addresses concentrated near the region boundaries (0, span ends,
    // slot bases, the unmapped tail) plus wild addresses
    // The PARAMBUF seams are anchored here too (item 4). The lesson written at
    // the ENGINE1 arm of zref::MemoryGuard is that this fuzz found nothing about
    // RENDER.ASSET_POOL for weeks because its anchors were all in framebuffer
    // space -- a divergence at addresses the test never generates does not exist
    // as far as the test is concerned. The three seams are where a union bug
    // lives, so they are where the jitter is aimed.
    const uint32_t anchors[] = {0x00000000, 0x0003BFC0, 0x0003C000, 0x00077FC0, 0x00078000,
                                0x0007FFFF, 0x01FFFFC0, 0x02000000, 0x0203BFC0, 0x0203C000,
                                0x05FFFFC0, 0x06000000, 0x063FFFC0, 0x06400000, 0x067FFFC0,
                                0x06800000, 0x069FFFC0, 0x069FFFF0, 0x06A00000, 0x07FFFFC0,
                                0x08000000};
    constexpr unsigned NANCHORS = sizeof(anchors) / sizeof(anchors[0]);
    const unsigned NFUZZ = 2000;
    for (unsigned i = 0; i < NFUZZ; i++) {
      const uint32_t base = anchors[pcg.range(NANCHORS)];
      const uint32_t addr = base + pcg.range(200) - 100;  // signed-ish jitter
      GuardMap m;
      m.valid = pcg.range(4) != 0;
      m.blit_slot = pcg.range(2);
      m.blit_span = 0x0003C000 - pcg.range(4) * 0x1000;
      // R32's region, drawn from the same anchors so its edges meet the pool's
      m.res_valid = pcg.range(2) != 0;
      m.res_base = anchors[pcg.range(NANCHORS)] + pcg.range(0x200) - 0x100;
      m.res_span = pcg.range(4) == 0 ? pcg.next() : 0x40u * (1 + pcg.range(0x80));
      // Item 4's three lease inputs, drawn independently, because the interesting
      // combinations are the incoherent ones: a scratch acquired without a lease,
      // a selector naming a view the request is not in. The directed cases above
      // pick those deliberately; this picks them 2,000 times without being told
      // which ones matter.
      m.pb_lease_valid = pcg.range(4) != 0;
      m.pb_wr_view = pcg.range(2);
      m.pb_scratch_valid = pcg.range(2) != 0;
      MemoryGuard::Req r;
      r.valid = true;
      r.write = pcg.range(2) == 0;
      r.client = pcg.range(7);
      r.addr = addr & 0x07FFFFFF;
      r.len = 1 + pcg.range(72);  // includes illegal >64
      if (r.len > 64 && pcg.range(2)) r.len = 64;
      r.be = (pcg.range(8) == 0) ? full_be(r.len > 64 ? 64 : r.len)
                                 : pcg.next() & ~0ull;  // sometimes holes
      h.request(r, m);
    }
  }

  // ---- totals: every verdict matched the oracle -------------------------------
  {
    // expected counts: recompute below (oracle tally from the calls above)
    // (request() tallied expect_ok/expect_viol as it ran)
    chk(h.saw_ok == h.expect_ok, "ok verdicts == oracle", h.expect_ok, h.saw_ok);
    chk(h.saw_viol == h.expect_viol, "violation verdicts == oracle", h.expect_viol, h.saw_viol);
    chk(h.top.guard_violations == h.expect_viol, "guard_violations counted", h.expect_viol,
        h.top.guard_violations);
    chk(h.mismatches == 0, "every request accepted once and received one exact verdict", 0,
        h.mismatches);
  }

  // drain: model still clean
  for (int i = 0; i < 200; i++) h.tick();
  chk(h.top.model_error == 0, "model timing clean through the guard");

  h.top.final();
  std::printf("mem_guard_directed: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
  zhao::exit_hard(failures ? 1 : 0);  // teardown-deadlock workaround (zhao_sim.hpp)
}
