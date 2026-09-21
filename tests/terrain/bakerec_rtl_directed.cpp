// bakerec_rtl_directed.cpp -- TERRAIN.BAKEREC, the patch-bake record producer.
//
// ===========================================================================
// WHAT IS AT RISK, AND WHY EACH CHECK IS THE SHAPE IT IS
// ===========================================================================
// This block can be wrong in five ways that all produce a machine that looks
// like it is working:
//
//   * A RECORD ISSUED BEFORE THE PAGE FACE IS LIVE. `zhao_terrain_pageio`'s
//     `nb_o` is combinational on a shadow plane that does not exist until
//     S_SERVE, so a dig started early reads an EMPTY no-bake plane: section
//     3.3's corner shadow silently vanishes and every handshake and every
//     counter agrees. Case 6 holds `io_serving_i` low and requires the seam to
//     be offered NOTHING.
//   * A COALESCE THAT LOSES A STAMP. Two stamps on one patch must become one
//     record carrying the SECOND one's geometry -- the delta law makes that
//     exact -- and a coalesce that kept the FIRST geometry would dig the wrong
//     place with `coalesced_o` reading a perfectly healthy 1. Case 3 checks
//     the FIELDS, not the counter.
//   * A FALLBACK THAT SILENTLY DROPS A SCAR. R221's ST_MISS fallback digs
//     nothing here by design, so the record MUST come back. A retry that
//     quietly did not re-queue would leave `records_retried_o` correct and the
//     deformation gone. Case 7 requires the record to be ISSUED AGAIN, and
//     case 8 requires the drop to be LOUD once `RETRIES` is spent.
//   * A QUEUE THAT NEVER DRAINS. A stamp on a patch the camera never composes
//     would hold an entry for the life of the machine. Case 9 ages one out.
//   * A COUNTER THAT CANNOT DISCRIMINATE (R95). Every one of the eleven is
//     asserted SILENT before it is fired, in the same case, so a counter that
//     counted the wrong event would fail on the silent half.
//
// THE DUT IS THE REAL MODULE AT ITS OWN PORTS. There is no bench wrapper: this
// block has no submodule, its neighbours are all ready/valid, and playing them
// from C++ is what lets a case hold `io_serving_i` low -- which no composition
// of the real blocks can do on purpose.
//
// R60: this builds and runs.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_bakerec.h"

#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_fail = 0;

void ckv(bool ok, const char* what, long expect, long actual) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, actual);
  }
}

// The block's own parameter defaults, mirrored here so a case that depends on
// one says which. Changing a default without changing this file makes the
// affected case fail rather than pass quietly.
constexpr int kDepth = 2;
constexpr int kRetries = 3;
constexpr int kMaxAge = 8;
constexpr int kDualBit = 3;

struct World {
  Vzhao_terrain_bakerec* d;

  void tick() {
    d->clk = 0;
    d->eval();
    d->clk = 1;
    d->eval();
  }
  void idle(int n) {
    for (int i = 0; i < n; ++i) tick();
  }
  void reset() {
    d->rst_n = 0;
    d->clk = 0;
    d->frame_start_i = 0;
    d->st_fire_i = 0;
    d->st_patch_valid_i = 0;
    d->st_handle_i = 0;
    d->st_patch_ix_i = 0;
    d->st_patch_iz_i = 0;
    d->st_cx_i = 0;
    d->st_cz_i = 0;
    d->st_radius_i = 0;
    d->st_env_x0_i = 0;
    d->st_env_z0_i = 0;
    d->st_env_x1_i = 0;
    d->st_env_z1_i = 0;
    d->st_src_id_i = 0;
    d->pg_fire_i = 0;
    d->pg_ix_i = 0;
    d->pg_iz_i = 0;
    d->pg_slot_i = 0;
    d->pg_gen_i = 0;
    d->pg_epoch_i = 0;
    d->pg_flags_i = 0;
    d->io_ready_i = 0;
    d->io_serving_i = 0;
    d->io_done_valid_i = 0;
    d->job_ready_i = 0;
    d->bk_valid_i = 0;
    d->bk_ready_i = 0;
    d->bk_fallback_i = 0;
    d->ps_j_ready_i = 0;
    d->ps_done_valid_i = 0;
    d->bake_done_i = 0;
    idle(4);
    d->rst_n = 1;
    idle(2);
  }

  // A stamp, presented for exactly one accepted cycle.
  void stamp(int16_t ix, int16_t iz, uint32_t handle, int32_t cx, int32_t cz,
             int32_t radius, uint16_t src, bool placed = true) {
    d->st_fire_i = 1;
    d->st_patch_valid_i = placed ? 1 : 0;
    d->st_handle_i = handle;
    d->st_patch_ix_i = ix;
    d->st_patch_iz_i = iz;
    d->st_cx_i = cx;
    d->st_cz_i = cz;
    d->st_radius_i = radius;
    d->st_env_x0_i = cx - 1000;
    d->st_env_z0_i = cz - 1000;
    d->st_env_x1_i = cx + 1000;
    d->st_env_z1_i = cz + 1000;
    d->st_src_id_i = src;
    tick();
    d->st_fire_i = 0;
    d->st_patch_valid_i = 0;
    d->eval();
  }

  // The page identity, as TERRAIN.HDRREAD's forward handshake delivers it.
  void page(int16_t ix, int16_t iz, uint16_t slot, uint8_t gen, uint32_t epoch,
            uint16_t flags) {
    d->pg_fire_i = 1;
    d->pg_ix_i = ix;
    d->pg_iz_i = iz;
    d->pg_slot_i = slot;
    d->pg_gen_i = gen;
    d->pg_epoch_i = epoch;
    d->pg_flags_i = flags;
    tick();
    d->pg_fire_i = 0;
    d->eval();
  }

  void frame() {
    d->frame_start_i = 1;
    tick();
    d->frame_start_i = 0;
    d->eval();
  }

  // Wait for a level, bounded. Returns the cycles spent, or -1.
  int wait_for(const CData& sig, int limit = 200) {
    for (int i = 0; i < limit; ++i) {
      d->eval();
      if (sig) return i;
      tick();
    }
    return -1;
  }
};

// Walk one record all the way through, playing the three agents. `fallback`
// is what the seam reports at bake's accept. Returns false if the block never
// offered the page job.
bool drive_record(World& w, bool fallback, int* io_wait = nullptr) {
  Vzhao_terrain_bakerec* d = w.d;

  // 1. PAGEIO's job.
  if (w.wait_for(d->io_valid_o) < 0) return false;
  d->io_ready_i = 1;
  w.tick();
  d->io_ready_i = 0;
  d->eval();

  // 2. THE INTERLOCK. Nothing may be offered to the seam until the page face
  //    is live. Measured here rather than assumed.
  int spent = 0;
  for (int i = 0; i < 20; ++i) {
    d->eval();
    if (d->job_valid_o) break;
    ++spent;
    w.tick();
  }
  if (io_wait) *io_wait = spent;
  d->io_serving_i = 1;
  if (w.wait_for(d->job_valid_o) < 0) return false;

  // 3. The seam accepts once its prefetch is done.
  d->job_ready_i = 1;
  w.tick();
  d->job_ready_i = 0;
  d->eval();

  // 4. The lattice pass.
  if (w.wait_for(d->ps_j_valid_o) < 0) return false;
  d->ps_j_ready_i = 1;
  w.tick();
  d->ps_j_ready_i = 0;
  d->eval();

  // 5. The seam hands the record to bake, with R221's verdict on it.
  d->bk_valid_i = 1;
  d->bk_ready_i = 1;
  d->bk_fallback_i = fallback ? 1 : 0;
  w.tick();
  d->bk_valid_i = 0;
  d->bk_ready_i = 0;
  d->bk_fallback_i = 0;
  d->eval();

  // 6. The three completions. The stream retires at the END OF THE LATTICE,
  //    which is the end of the DIG phase -- before BREACH and therefore before
  //    `bake_done`. Delivered in that order on purpose.
  d->ps_done_valid_i = 1;
  w.tick();
  d->ps_done_valid_i = 0;
  d->bake_done_i = 1;
  w.tick();
  d->bake_done_i = 0;
  d->io_done_valid_i = 1;
  w.tick();
  d->io_done_valid_i = 0;
  d->io_serving_i = 0;
  w.idle(4);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* dut = new Vzhao_terrain_bakerec;
  World w{dut};
  Vzhao_terrain_bakerec& d = *dut;

  // =========================================================================
  // 1. RESET IS QUIET. Every counter zero and the block idle.
  // =========================================================================
  w.reset();
  ckv(d.idle_o == 1, "1 idle after reset", 1, d.idle_o);
  ckv(d.io_valid_o == 0, "1 no page job after reset", 0, d.io_valid_o);
  ckv(d.job_valid_o == 0, "1 no seam job after reset", 0, d.job_valid_o);
  ckv(d.stamps_seen_o == 0, "1 stamps_seen silent", 0, long(d.stamps_seen_o));
  ckv(d.records_queued_o == 0, "1 records_queued silent", 0,
      long(d.records_queued_o));
  ckv(d.coalesced_o == 0, "1 coalesced silent", 0, long(d.coalesced_o));
  ckv(d.overflow_o == 0, "1 overflow silent", 0, long(d.overflow_o));
  ckv(d.unplaced_o == 0, "1 unplaced silent", 0, long(d.unplaced_o));
  ckv(d.records_issued_o == 0, "1 records_issued silent", 0,
      long(d.records_issued_o));
  ckv(d.records_retired_o == 0, "1 records_retired silent", 0,
      long(d.records_retired_o));
  ckv(d.records_retried_o == 0, "1 records_retried silent", 0,
      long(d.records_retried_o));
  ckv(d.records_dropped_o == 0, "1 records_dropped silent", 0,
      long(d.records_dropped_o));
  ckv(d.aged_out_o == 0, "1 aged_out silent", 0, long(d.aged_out_o));
  ckv(d.stray_done_o == 0, "1 stray_done silent", 0, long(d.stray_done_o));

  // =========================================================================
  // 2. AN UNPLACED STAMP IS COUNTED AND DROPPED. `zhao_surface_dispatch`
  //    REFUSES a pitch outside [-1, 2] rather than clamping it, and a record
  //    built on a refused placement would bake a patch that is not there.
  // =========================================================================
  w.stamp(3, 5, 0xAAAA0001u, 1000, 2000, 500, 0x11, /*placed=*/false);
  ckv(d.stamps_seen_o == 1, "2 the stamp was seen", 1, long(d.stamps_seen_o));
  ckv(d.unplaced_o == 1, "2 unplaced FIRED", 1, long(d.unplaced_o));
  ckv(d.records_queued_o == 0, "2 and nothing was queued", 0,
      long(d.records_queued_o));

  // =========================================================================
  // 3. COALESCING KEEPS THE SECOND GEOMETRY, and the fields say so rather
  //    than the counter. Under owner ruling R231 one dig of (after - before)
  //    is the sum of both stamps exactly, so one record is not an
  //    approximation -- but only if it is the LIVE geometry.
  // =========================================================================
  w.stamp(3, 5, 0xAAAA0001u, 1000, 2000, 500, 0x11);
  ckv(d.records_queued_o == 1, "3 the first stamp opened a record", 1,
      long(d.records_queued_o));
  ckv(d.coalesced_o == 0, "3 coalesced still silent", 0, long(d.coalesced_o));
  w.stamp(3, 5, 0xAAAA0002u, 1700, 2700, 900, 0x22);
  ckv(d.coalesced_o == 1, "3 coalesced FIRED on the same patch key", 1,
      long(d.coalesced_o));
  ckv(d.records_queued_o == 1, "3 and no second record was opened", 1,
      long(d.records_queued_o));

  // The page identity, with kFlagDual SET so `cmd_dual_o` has something to
  // carry that is not its reset value.
  w.page(3, 5, 41, 7, 0xC0FFEEu, uint16_t(1u << kDualBit));

  {
    int io_wait = -1;
    const bool ok = drive_record(w, /*fallback=*/false, &io_wait);
    ckv(ok, "3 the record walked the whole chain", 1, ok ? 1 : 0);
  }
  ckv(d.records_issued_o == 1, "3 records_issued FIRED", 1,
      long(d.records_issued_o));
  ckv(d.records_retired_o == 1, "3 records_retired FIRED", 1,
      long(d.records_retired_o));
  ckv(d.records_retried_o == 0, "3 records_retried still silent", 0,
      long(d.records_retried_o));
  ckv(d.records_dropped_o == 0, "3 records_dropped still silent", 0,
      long(d.records_dropped_o));

  // =========================================================================
  // 4. THE RECORD's FIELDS, CHECKED WHILE IT IS PRESENTED. Re-issued so they
  //    can be read at the seam's offer, which is where the producer's
  //    ready/valid contract requires them to be stable.
  // =========================================================================
  w.stamp(3, 5, 0xAAAA0002u, 1700, 2700, 900, 0x22);
  w.page(3, 5, 41, 7, 0xC0FFEEu, uint16_t(1u << kDualBit));
  ckv(w.wait_for(d.io_valid_o) >= 0, "4 the page job was offered", 1, 1);
  ckv(long(d.io_slot_o) == 41, "4 io_slot carries the page's slot", 41,
      long(d.io_slot_o));
  ckv(long(d.io_gen_o) == 7, "4 io_gen carries the page's generation", 7,
      long(d.io_gen_o));
  ckv(long(d.io_epoch_o) == 0xC0FFEE, "4 io_epoch carries the page's epoch",
      0xC0FFEE, long(d.io_epoch_o));
  d.io_ready_i = 1;
  w.tick();
  d.io_ready_i = 0;

  // =========================================================================
  // 5. THE INTERLOCK. `io_serving_i` LOW must offer the seam NOTHING. This is
  //    the check that cannot be made by composing the real blocks, because
  //    nothing in the real chain can hold the page face dark on purpose.
  // =========================================================================
  for (int i = 0; i < 25; ++i) {
    w.tick();
    d.eval();
    if (d.job_valid_o) break;
  }
  ckv(d.job_valid_o == 0,
      "5 THE SEAM IS OFFERED NOTHING while the page face is dark", 0,
      d.job_valid_o);
  ckv(d.ps_j_valid_o == 0, "5 and no lattice pass was started either", 0,
      d.ps_j_valid_o);
  d.io_serving_i = 1;
  ckv(w.wait_for(d.job_valid_o) >= 0, "5 and it is offered once serving", 1, 1);

  ckv(long(d.job_handle_o) == long(0xAAAA0002u),
      "5 the handle is the SECOND stamp's, CARRIED not derived", 0xAAAA0002,
      long(d.job_handle_o));
  ckv(d.job_want_sheet_o == 1, "5 every record asks for the sheet law", 1,
      d.job_want_sheet_o);
  ckv(long(d.job_src_id_o) == 0x22, "5 the src_id is the second stamp's", 0x22,
      long(d.job_src_id_o));
  ckv(long(d.cmd_cx_o) == 1700, "5 cmd_cx is the SECOND stamp's centre", 1700,
      long(d.cmd_cx_o));
  ckv(long(d.cmd_cz_o) == 2700, "5 cmd_cz is the SECOND stamp's centre", 2700,
      long(d.cmd_cz_o));
  ckv(long(d.cmd_radius_o) == 900, "5 cmd_radius is the SECOND stamp's", 900,
      long(d.cmd_radius_o));
  ckv(long(d.cmd_env_x0_o) == 700, "5 the envelope is carried", 700,
      long(d.cmd_env_x0_o));
  ckv(long(d.cmd_env_x1_o) == 2700, "5 the envelope is carried", 2700,
      long(d.cmd_env_x1_o));
  ckv(d.cmd_dual_o == 1, "5 cmd_dual is the page's own kFlagDual", 1,
      d.cmd_dual_o);
  ckv(d.cmd_cells_o == 1, "5 cmd_cells says layer D is served", 1,
      d.cmd_cells_o);
  ckv(long(d.cmd_src_id_o) == 0x22, "5 cmd_src_id is carried", 0x22,
      long(d.cmd_src_id_o));
  // THE DISC DEPTHS. Zero is the DECISION, not an oversight -- see the block's
  // header and design/contracts/TERRAIN.BAKEREC.md section 4. Asserted so that
  // changing FALLBACK_DEPTH_* is a deliberate act with a failing test beside
  // it rather than a silent change of the dig law.
  ckv(long(d.cmd_depth_from_o) == 0,
      "5 cmd_depth_from is FALLBACK_DEPTH_FROM (R231: a fallback defers, it "
      "does not dig an absolute depth into an accumulating scar)",
      0, long(d.cmd_depth_from_o));
  ckv(long(d.cmd_depth_to_o) == 0, "5 cmd_depth_to is FALLBACK_DEPTH_TO", 0,
      long(d.cmd_depth_to_o));
  ckv(long(d.cmd_patch_id_o) == ((3 << 8) | 5),
      "5 cmd_patch_id is the TRACE key {ix[7:0], iz[7:0]}", (3 << 8) | 5,
      long(d.cmd_patch_id_o));

  // Let it finish.
  d.job_ready_i = 1;
  w.tick();
  d.job_ready_i = 0;
  w.wait_for(d.ps_j_valid_o);
  ckv(long(d.ps_j_slot_o) == 41, "5 the lattice pass uses the same slot", 41,
      long(d.ps_j_slot_o));
  ckv(long(d.ps_j_flags_o) == (1 << kDualBit),
      "5 and carries the record flags -- NOT the mip pass's zero",
      1 << kDualBit, long(d.ps_j_flags_o));
  d.ps_j_ready_i = 1;
  w.tick();
  d.ps_j_ready_i = 0;
  d.bk_valid_i = 1;
  d.bk_ready_i = 1;
  w.tick();
  d.bk_valid_i = 0;
  d.bk_ready_i = 0;
  d.ps_done_valid_i = 1;
  w.tick();
  d.ps_done_valid_i = 0;
  d.bake_done_i = 1;
  w.tick();
  d.bake_done_i = 0;
  d.io_done_valid_i = 1;
  w.tick();
  d.io_done_valid_i = 0;
  d.io_serving_i = 0;
  w.idle(4);
  ckv(d.idle_o == 1, "5 the block is idle again", 1, d.idle_o);

  // =========================================================================
  // 6. STRAY COMPLETION. A `bake_done` with nothing in flight is REACHABLE --
  //    a completion arriving after a retirement -- so it is a counter and not
  //    an assertion, which is the distinction zhao_terrain_pageio draws.
  // =========================================================================
  ckv(d.stray_done_o == 0, "6 stray_done still silent", 0,
      long(d.stray_done_o));
  d.bake_done_i = 1;
  w.tick();
  d.bake_done_i = 0;
  w.idle(2);
  ckv(d.stray_done_o == 1, "6 stray_done FIRED", 1, long(d.stray_done_o));

  // =========================================================================
  // 7. R221's FALLBACK IS A DEFERRAL. The record must come BACK, not vanish.
  //    A retry that counted and did not re-queue would leave
  //    `records_retried_o` correct and the deformation gone -- which is the
  //    shape of every defect this subsystem has shipped.
  // =========================================================================
  w.reset();
  w.stamp(9, 9, 0xBBBB0001u, 40, 50, 60, 0x33);
  w.page(9, 9, 12, 3, 0x1234u, 0);
  {
    const long issued0 = d.records_issued_o;
    ckv(drive_record(w, /*fallback=*/true), "7 the record walked the chain", 1,
        1);
    ckv(d.records_retried_o == 1, "7 records_retried FIRED", 1,
        long(d.records_retried_o));
    ckv(d.records_retired_o == 0, "7 and it did NOT retire", 0,
        long(d.records_retired_o));
    ckv(d.records_dropped_o == 0, "7 and it was NOT dropped", 0,
        long(d.records_dropped_o));
    // THE CHECK THAT MATTERS: it is offered AGAIN, with its page identity and
    // its geometry intact.
    ckv(w.wait_for(d.io_valid_o) >= 0,
        "7 THE RECORD CAME BACK -- the fallback is a deferral, not a loss", 1,
        1);
    ckv(long(d.io_slot_o) == 12, "7 with the same page identity", 12,
        long(d.io_slot_o));
    // `records_issued_o` counts the SEAM's ACCEPT, so a re-issued record is
    // issued again and the two counters are deliberately not the same number:
    // issued counts ATTEMPTS, retired+dropped count RECORDS. Written out
    // because "issued == retired" is the intuitive invariant and it is false.
    ckv(long(d.records_issued_o) == issued0 + 1,
        "7 records_issued counts ATTEMPTS, so the fallback attempt is in it",
        issued0 + 1, long(d.records_issued_o));
  }

  // Two more fallbacks spend RETRIES; the third retirement must DROP.
  for (int i = 1; i < kRetries; ++i) {
    ckv(drive_record(w, /*fallback=*/true), "8 a further fallback", 1, 1);
  }
  ckv(long(d.records_retried_o) == kRetries, "8 RETRIES re-issues were counted",
      kRetries, long(d.records_retried_o));
  ckv(d.records_dropped_o == 0, "8 records_dropped still silent", 0,
      long(d.records_dropped_o));
  ckv(drive_record(w, /*fallback=*/true), "8 the last attempt", 1, 1);
  ckv(d.records_dropped_o == 1,
      "8 records_dropped FIRED -- a scar that never found its sheet is LOUD", 1,
      long(d.records_dropped_o));
  ckv(d.idle_o == 1, "8 and the queue drained", 1, d.idle_o);

  // =========================================================================
  // 9. THE AGE-OUT. A stamp on a patch the compose engine never visits must
  //    not hold a queue entry for the life of the machine.
  // =========================================================================
  w.reset();
  w.stamp(20, 21, 0xCCCC0001u, 1, 2, 3, 0x44);
  for (int i = 0; i <= kMaxAge; ++i) {
    ckv(d.aged_out_o == 0, "9 aged_out silent while the record is young", 0,
        long(d.aged_out_o));
    w.frame();
    w.idle(2);
  }
  ckv(d.aged_out_o == 1, "9 aged_out FIRED after MAX_AGE frames", 1,
      long(d.aged_out_o));
  // And the entry is gone: a new patch must fit even though the queue is only
  // `kDepth` deep.
  ckv(d.records_queued_o == 1, "9 one record was ever queued", 1,
      long(d.records_queued_o));

  // =========================================================================
  // 10. OVERFLOW. `kDepth` different patches fit; the next one is a LOST scar
  //     and says so. Reaching it needs three DIFFERENT patches, because
  //     stamps on one patch coalesce.
  // =========================================================================
  w.reset();
  for (int k = 0; k < kDepth; ++k) {
    w.stamp(int16_t(30 + k), int16_t(40 + k), 0xDDDD0000u + k, k, k, k,
            uint16_t(0x50 + k));
  }
  ckv(long(d.records_queued_o) == kDepth, "10 the queue took DEPTH records",
      kDepth, long(d.records_queued_o));
  ckv(d.overflow_o == 0, "10 overflow silent while there was room", 0,
      long(d.overflow_o));
  w.stamp(99, 99, 0xDDDD9999u, 7, 7, 7, 0x59);
  ckv(d.overflow_o == 1, "10 overflow FIRED on the third distinct patch", 1,
      long(d.overflow_o));
  ckv(long(d.records_queued_o) == kDepth, "10 and nothing was queued for it",
      kDepth, long(d.records_queued_o));
  ckv(long(d.stamps_seen_o) == kDepth + 1, "10 every stamp was still SEEN",
      kDepth + 1, long(d.stamps_seen_o));

  // A page for one of the two held records must still issue it -- the
  // overflowed stamp must not have disturbed the queue.
  w.page(31, 41, 77, 2, 0x99u, 0);
  ckv(w.wait_for(d.io_valid_o) >= 0, "10 a held record still issues", 1, 1);
  ckv(long(d.io_slot_o) == 77, "10 with the right page", 77, long(d.io_slot_o));

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);
  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  zhao::exit_hard(rc);
}
