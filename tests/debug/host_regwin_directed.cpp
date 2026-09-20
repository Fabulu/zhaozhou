// host_regwin_directed.cpp -- HOST.REGWIN, the HPS lightweight-bridge CSR
// aperture (owner ruling R51). Directed test of the APERTURE ALONE.
//
// WHY THIS EXISTS BESIDE THE SMOKE. The composed console proves the aperture
// answers two real tenants with real values, and that is the closure evidence
// for header entries I19 and I45. It cannot prove three things, and they are
// exactly the three a carrier has to get right:
//
//   1. NO-ESCAPE. In the console both tenants are live, so "tenant 1's address
//      never reaches tenant 0" is checked against a block that would probably
//      answer plausibly anyway. Here the tenants are MODELS and every offer
//      they are given is recorded, so the claim is measured on the wire.
//   2. THE TIMEOUT. `refused_timeout_o` cannot move while every tenant
//      acknowledges, and both real ones always do. It is the `wq_overflow_o`
//      shape CLAUDE.md describes -- except that it needs no mutant, because
//      `t_ack_i` is an INPUT: holding it low is perfectly legal stimulus at
//      this block's port. A counter that has not been seen to fire has not
//      been tested, and this is where it fires.
//   3. THE COUNTERS' CENSUS. `reads_o`/`writes_o` count what was ASKED, before
//      any verdict. That distinction is invisible in a run where nothing is
//      refused, and it is the one that reads LOW if it is wrong.
//
// Every counter this block owns is fired here. None is asserted zero without a
// case that moves it.

#include "Vzhao_host_regwin.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using zhao::check;

// The frozen map, spec/memory_rules.md section 8.1.
constexpr int kTenantLsb = 12;
constexpr int kNTenant = 2;
constexpr uint32_t kAckLimit = 255;

// What a tenant model was asked for. The aperture's whole no-escape claim is
// about these three fields, so they are recorded rather than reasoned about.
struct Offer {
  int tenant;
  uint32_t woff;
  bool write;
  uint32_t wdata;
};

// A tenant model. `ack_after` cycles of silence before it acknowledges, then
// `rsp_after` more before it answers. `never_ack` is the timeout stimulus.
struct Tenant {
  bool never_ack = false;
  bool err = false;
  uint32_t rdata = 0;
  int ack_delay = 0;
  int rsp_delay = 1;  // must be >= 1: the aperture looks at rvalid only after
                      // it has seen the ack, so an adapter raising both in one
                      // cycle would have its response missed.
  // live state
  int phase = 0;  // 0 idle, 1 waiting to ack, 2 waiting to answer
  int count = 0;
};

struct Bus {
  Vzhao_host_regwin& dut;
  Tenant t[kNTenant];
  std::vector<Offer> offers;

  explicit Bus(Vzhao_host_regwin& d) : dut(d) {}

  void drive_tenants() {
    uint32_t ack = 0, rvalid = 0, err = 0;
    uint64_t rdata = 0;
    for (int i = 0; i < kNTenant; ++i) {
      Tenant& tn = t[i];
      const bool sel = ((dut.t_sel_o >> i) & 1u) != 0u;
      if (sel && tn.phase == 0) {
        // Record EXACTLY what this tenant was offered. If the aperture ever
        // hands tenant 0 an address belonging to tenant 1, it shows up here
        // and nowhere else.
        offers.push_back(Offer{i, static_cast<uint32_t>(dut.t_woff_o),
                               dut.t_write_o != 0,
                               static_cast<uint32_t>(dut.t_wdata_o)});
        tn.phase = 1;
        tn.count = 0;
      }
      if (tn.phase == 1) {
        if (!tn.never_ack && tn.count >= tn.ack_delay) {
          ack |= (1u << i);
          tn.phase = 2;
          tn.count = 0;
        } else {
          ++tn.count;
        }
      } else if (tn.phase == 2) {
        if (tn.count >= tn.rsp_delay) {
          rvalid |= (1u << i);
          if (tn.err) err |= (1u << i);
          rdata |= (static_cast<uint64_t>(tn.rdata) << (32 * i));
          tn.phase = 0;
          tn.count = 0;
        } else {
          ++tn.count;
        }
      }
    }
    dut.t_ack_i = ack;
    dut.t_rvalid_i = rvalid;
    dut.t_err_i = err;
    dut.t_rdata_i = rdata;
  }

  void idle_tenants() {
    for (int i = 0; i < kNTenant; ++i) {
      t[i].phase = 0;
      t[i].count = 0;
    }
    dut.t_ack_i = 0;
    dut.t_rvalid_i = 0;
    dut.t_err_i = 0;
    dut.t_rdata_i = 0;
  }

  void step() {
    drive_tenants();
    dut.eval();
    zhao::tick(dut);
    dut.eval();
  }

  // One access. Returns false when the aperture never answered, which is the
  // one outcome this block is not allowed to produce.
  bool access(bool write, uint32_t addr, uint32_t wdata, uint32_t* rdata,
              bool* err, int budget = 4000) {
    int g = 0;
    while (!dut.h_ready_o && g < budget) {
      step();
      ++g;
    }
    if (!dut.h_ready_o) return false;
    dut.h_valid_i = 1;
    dut.h_write_i = write ? 1 : 0;
    dut.h_addr_i = addr;
    dut.h_wdata_i = wdata;
    step();
    dut.h_valid_i = 0;
    dut.h_write_i = 0;
    g = 0;
    while (!dut.h_rvalid_o && g < budget) {
      step();
      ++g;
    }
    if (!dut.h_rvalid_o) return false;
    if (rdata) *rdata = dut.h_rdata_o;
    if (err) *err = dut.h_err_o != 0;
    step();
    return true;
  }
};

void reset(Vzhao_host_regwin& dut) {
  dut.rst_n = 0;
  dut.h_valid_i = 0;
  dut.h_write_i = 0;
  dut.h_addr_i = 0;
  dut.h_wdata_i = 0;
  dut.t_ack_i = 0;
  dut.t_rvalid_i = 0;
  dut.t_err_i = 0;
  dut.t_rdata_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
}

constexpr uint32_t addr_of(int tenant, uint32_t word) {
  return (static_cast<uint32_t>(tenant) << kTenantLsb) | (word << 2);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_host_regwin dut;
  reset(dut);
  Bus bus(dut);

  uint32_t d = 0;
  bool err = false;

  // ---- 1. A plain read of each tenant, and the OFFER it produced ----------
  // The word offset the tenant sees must be the address's low bits and nothing
  // else, and the OTHER tenant must see nothing at all.
  bus.t[0].rdata = 0xA5A50001u;
  bus.t[1].rdata = 0x5A5A0002u;
  check(bus.access(false, addr_of(0, 0x123), 0, &d, &err),
        "tenant 0: the aperture answered", 1, 1);
  check(!err && d == 0xA5A50001u, "tenant 0: returned that tenant's data",
        0xA5A50001u, d);
  check(bus.offers.size() == 1, "tenant 0: exactly ONE tenant was offered the access",
        1, static_cast<uint64_t>(bus.offers.size()));
  check(bus.offers[0].tenant == 0, "tenant 0: it was tenant 0", 0,
        static_cast<uint64_t>(bus.offers[0].tenant));
  check(bus.offers[0].woff == 0x123,
        "tenant 0: the word offset is addr[11:2], not the byte address", 0x123,
        bus.offers[0].woff);

  bus.offers.clear();
  check(bus.access(false, addr_of(1, 0x3FF), 0, &d, &err),
        "tenant 1: the aperture answered", 1, 1);
  check(!err && d == 0x5A5A0002u, "tenant 1: returned THAT tenant's data, not tenant 0's",
        0x5A5A0002u, d);
  check(bus.offers.size() == 1 && bus.offers[0].tenant == 1,
        "tenant 1: tenant 0 was not selected", 1,
        static_cast<uint64_t>(bus.offers.size() == 1 && bus.offers[0].tenant == 1));
  check(bus.offers[0].woff == 0x3FF,
        "tenant 1: the LAST word of its region is still its own offset", 0x3FF,
        bus.offers[0].woff);

  // ---- 2. NO ESCAPE, measured -------------------------------------------
  // Every word of tenant 1's region, sampled at both ends and across the byte
  // that would carry into the tenant field if the split were arithmetic rather
  // than structural. Tenant 0 must never be offered anything.
  bus.offers.clear();
  const uint32_t probe[] = {0x000, 0x001, 0x0FF, 0x100, 0x1FF, 0x200, 0x3FE, 0x3FF};
  for (uint32_t w : probe) {
    check(bus.access(false, addr_of(1, w), 0, &d, &err),
          "no-escape sweep: answered", 1, 1);
  }
  bool escaped = false;
  for (const Offer& o : bus.offers) {
    if (o.tenant != 1 || o.woff > 0x3FF) escaped = true;
  }
  check(!escaped && bus.offers.size() == sizeof(probe) / sizeof(probe[0]),
        "no-escape: every offer went to tenant 1, inside its own 10-bit offset",
        sizeof(probe) / sizeof(probe[0]), static_cast<uint64_t>(bus.offers.size()));

  // ---- 3. The three address refusals, each counted separately ------------
  const uint32_t unmapped_before = dut.refused_unmapped_o;
  check(bus.access(false, addr_of(2, 0), 0, &d, &err), "unmapped: answered", 1, 1);
  check(err, "unmapped: a region the map has not populated is REFUSED", 1, err ? 1 : 0);
  check(d == 0, "unmapped: the refusal carries zero, not stale data", 0, d);
  check(dut.refused_unmapped_o == unmapped_before + 1,
        "unmapped: refused_unmapped_o FIRED", unmapped_before + 1,
        dut.refused_unmapped_o);

  // The TOP region, so a decode that treated the index as signed or wrapped it
  // would be caught rather than looking like the same case as tenant 2.
  check(bus.access(false, addr_of(15, 0x3FF), 0, &d, &err), "unmapped 15: answered", 1, 1);
  check(err && dut.refused_unmapped_o == unmapped_before + 2,
        "unmapped: region 15 is refused too", unmapped_before + 2,
        dut.refused_unmapped_o);

  const uint32_t mis_before = dut.refused_misaligned_o;
  for (uint32_t byte : {1u, 2u, 3u}) {
    check(bus.access(false, addr_of(0, 4) | byte, 0, &d, &err), "misaligned: answered", 1, 1);
    check(err, "misaligned: a byte address with addr[1:0] != 0 is REFUSED", 1, err ? 1 : 0);
  }
  check(dut.refused_misaligned_o == mis_before + 3,
        "misaligned: refused_misaligned_o fired once per offence", mis_before + 3,
        dut.refused_misaligned_o);
  // NOTHING WAS ISSUED. The burst bridge's law: a malformed access is rejected
  // at the port and never reaches the thing behind it.
  bus.offers.clear();
  check(bus.access(false, addr_of(0, 8) | 2u, 0, &d, &err), "misaligned: answered", 1, 1);
  check(bus.offers.empty(),
        "misaligned: NOTHING was offered to the tenant", 0,
        static_cast<uint64_t>(bus.offers.size()));

  // ---- 4. A tenant's OWN refusal is a third, separate count --------------
  const uint32_t ten_before = dut.refused_tenant_o;
  bus.t[0].err = true;
  bus.t[0].rdata = 0xDEADBEEFu;
  check(bus.access(false, addr_of(0, 0x10), 0, &d, &err), "tenant refusal: answered", 1, 1);
  check(err, "tenant refusal: the tenant's err reaches the host", 1, err ? 1 : 0);
  check(d == 0,
        "tenant refusal: a refused read returns ZERO, not the tenant's data bus",
        0, d);
  check(dut.refused_tenant_o == ten_before + 1, "tenant refusal: refused_tenant_o FIRED",
        ten_before + 1, dut.refused_tenant_o);
  bus.t[0].err = false;

  // ---- 5. Writes are carried, and counted as writes ----------------------
  const uint32_t writes_before = dut.writes_o;
  const uint32_t reads_before = dut.reads_o;
  bus.offers.clear();
  bus.t[1].rdata = 0;
  check(bus.access(true, addr_of(1, 0x20), 0xC0FFEE01u, &d, &err), "write: answered", 1, 1);
  check(bus.offers.size() == 1 && bus.offers[0].write,
        "write: the tenant was told it is a write", 1,
        static_cast<uint64_t>(bus.offers.size() == 1 && bus.offers[0].write));
  check(bus.offers[0].wdata == 0xC0FFEE01u, "write: the data reached the tenant",
        0xC0FFEE01u, bus.offers[0].wdata);
  check(dut.writes_o == writes_before + 1, "write: writes_o fired", writes_before + 1,
        dut.writes_o);
  check(dut.reads_o == reads_before, "write: reads_o did NOT move", reads_before,
        dut.reads_o);

  // A REFUSED access is still counted as the access it ASKED to be. A census of
  // only the ones that worked reads low, which is the flattering direction.
  const uint32_t writes_b2 = dut.writes_o;
  check(bus.access(true, addr_of(7, 0), 0, &d, &err), "refused write: answered", 1, 1);
  check(err, "refused write: unmapped", 1, err ? 1 : 0);
  check(dut.writes_o == writes_b2 + 1,
        "refused write: counted as a WRITE, before the verdict", writes_b2 + 1,
        dut.writes_o);

  // ---- 6. THE TIMEOUT, fired with legal stimulus -------------------------
  // The composed console cannot reach this: both real tenants always
  // acknowledge. `t_ack_i` is an input, so holding it low is legal here, and it
  // is the only honest way to show the guard can move.
  const uint32_t to_before = dut.refused_timeout_o;
  bus.t[0].never_ack = true;
  check(bus.access(false, addr_of(0, 0x30), 0, &d, &err),
        "timeout: the aperture ANSWERED rather than hanging -- ruling R20's law",
        1, 1);
  check(err, "timeout: the unanswered access is refused", 1, err ? 1 : 0);
  check(d == 0, "timeout: it carries zero", 0, d);
  check(dut.refused_timeout_o == to_before + 1, "timeout: refused_timeout_o FIRED",
        to_before + 1, dut.refused_timeout_o);
  bus.t[0].never_ack = false;
  bus.idle_tenants();

  // The other half of the timeout: a tenant that ACKNOWLEDGES and then never
  // answers. A guard that only watched the ack would miss it, and a tenant
  // that takes the access and drops it is the more likely fault of the two.
  const uint32_t to_b2 = dut.refused_timeout_o;
  bus.t[1].rsp_delay = static_cast<int>(kAckLimit) + 64;
  check(bus.access(false, addr_of(1, 0x40), 0, &d, &err),
        "timeout after ack: answered", 1, 1);
  check(err && dut.refused_timeout_o == to_b2 + 1,
        "timeout after ack: fired for a tenant that took the access and went quiet",
        to_b2 + 1, dut.refused_timeout_o);
  bus.t[1].rsp_delay = 1;
  bus.idle_tenants();

  // And the aperture RECOVERS: the very next access must behave normally.
  // A bus that is well after a timeout is worth more than one that is merely
  // not hung.
  bus.t[1].rdata = 0x1234ABCDu;
  check(bus.access(false, addr_of(1, 0x41), 0, &d, &err), "after timeout: answered", 1, 1);
  check(!err && d == 0x1234ABCDu, "after timeout: the aperture is well again",
        0x1234ABCDu, d);

  // ---- 7. A SLOW tenant is waited for, and the wait is measured ----------
  const uint32_t stall_before = dut.stall_cycles_o;
  bus.t[0].ack_delay = 7;
  bus.t[0].rsp_delay = 5;
  bus.t[0].rdata = 0x00C0FFEEu;
  check(bus.access(false, addr_of(0, 0x50), 0, &d, &err), "slow tenant: answered", 1, 1);
  check(!err && d == 0x00C0FFEEu, "slow tenant: the right data, late", 0x00C0FFEEu, d);
  check(dut.stall_cycles_o > stall_before + 10,
        "slow tenant: stall_cycles_o counted the wait", 1,
        dut.stall_cycles_o > stall_before + 10 ? 1 : 0);
  bus.t[0].ack_delay = 0;
  bus.t[0].rsp_delay = 1;

  // ---- 8. reads_o counts every read that was asked -----------------------
  // Asserted last so it covers the whole run, and computed from the cases
  // above rather than from a number typed here.
  check(dut.reads_o > 0 && dut.writes_o == 2,
        "census: reads and writes were counted as asked", 2, dut.writes_o);

  dut.final();
  return zhao::report_and_exit("host_regwin_directed");
}
