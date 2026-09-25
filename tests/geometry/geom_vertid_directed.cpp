// geom_vertid_directed.cpp -- GEOM.VERTID, the one geometry identity space.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 4;
//      design/contracts/GEOM.VERTID.md;
//      reference/include/zref/zref_fixp.hpp::unit8_from_fx16 (the colour law).
//
// SELF-CONSISTENCY BENCH, SAID OUT LOUD. This block is a sequencer and a table;
// it has no entry in the oracle and no ratified scalar law of its own except
// the colour quantisation, which IS differenced against zref below. Everything
// else is derived from the contract and the block header. The bench can prove
// the block does what the prose says; it cannot prove the prose is right.
//
// The bench plays the ALLOCATOR. `zhao_geom_paramarena` hands back
// `pv_accept_o` / `pv_id_o` on the clock it allocates, so the bench models
// exactly that: a dense cursor that advances only on an accepted record, and a
// SINK mode in which records are consumed and not allocated. That is the whole
// reason the block does not count vertices itself, so the model has to be able
// to disagree with a counter if the block ever grew one.
//
// THE CASE THE WHOLE SCHEME EXISTS FOR IS CASE 2: one vertex published once and
// referenced twice by two triangles that share an edge. Owner brief, ARENAID:
// "If you close I53, show a real vertex published once and referenced twice by
// two triangles sharing an edge -- that is the property the whole scheme exists
// for."
//
// EVERY COUNTER IS ASSERTED AS A DELTA ACROSS ITS OWN CASE, never as a nonzero
// total, and every one of the twelve is SEEN TO FIRE here. None of them owes a
// mutant, because all twelve are reachable with legal stimulus at this block's
// own ports -- which is the point of the port list being what it is.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_geom_vertid.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kArenas = 4;
constexpr int kArenaW = 3;
constexpr int kGenW = 8;
constexpr int kIndexW = 12;
constexpr int kVSlots = 64;
constexpr int kAttrs = 7;
// The block's ID_LIMIT default, and the count a u16 vertex_id names.
constexpr uint32_t kIdLimit = 65536;

constexpr int kDomMesh = 0;
constexpr int kDomForge = 1;
constexpr int kDomPart = 2;

uint32_t key_of(uint32_t arena, uint32_t gen, uint32_t index) {
  return (arena << (kGenW + kIndexW)) | (gen << kIndexW) | index;
}

// zref::unit8_from_fx16, transcribed. The RTL implements the same function and
// this is the independent copy the two are differenced against.
uint8_t unit8_from_fx16(int32_t r) {
  if (r < 0) return 0;
  if (r > 0xFFFF) return 255;
  int32_t q = (r + 128) >> 8;
  return static_cast<uint8_t>(q > 255 ? 255 : q);
}

struct Attr {
  uint32_t invw = 0;     // slot 0, low 24 bits
  uint32_t uow = 0;      // slot 1
  uint32_t vow = 0;      // slot 2
  uint32_t r = 0, g = 0, b = 0, a = 0;   // slots 3..6
};

struct Tri {
  int domain = kDomMesh;
  uint32_t key[3] = {0, 0, 0};
  int32_t x[3] = {0, 0, 0};
  int32_t y[3] = {0, 0, 0};
  Attr attr[3];
  bool untex = false;
  uint16_t material = 0;
  uint32_t raster = 0;
  uint16_t src_id = 0;
};

// A ProjectedVertex as it arrived at the allocator, kept so the bench can
// assert field for field what was published.
struct Pub {
  uint32_t id;
  int32_t x, y;
  uint32_t invw;
  uint8_t status;
  int32_t uow, vow;
  uint32_t rgba;
};

struct Env {
  Vzhao_geom_vertid* v;
  // ---- the allocator model ------------------------------------------------
  bool sealed = false;        // a frame is open at the allocator
  uint32_t vcursor = 0;       // its dense vertex cursor
  uint32_t tcursor = 0;       // its dense triangle cursor
  uint32_t forced_pv_id = 0;  // non-zero forces this id instead of the cursor
  bool force_pv_id = false;

  std::vector<Pub> pubs;                         // every accepted PV
  std::vector<std::array<uint16_t, 3>> descs;    // every accepted TD's ids
  std::vector<uint32_t> desc_ids;                // and the TD's own index

  explicit Env(Vzhao_geom_vertid* dut) : v(dut) {
    v->rst_n = 0;
    v->frame_seal_i = 0;
    v->tri_valid_i = 0;
    v->pv_ready_i = 0;
    v->pv_accept_i = 0;
    v->pv_id_i = 0;
    v->td_ready_i = 0;
    v->td_accept_i = 0;
    v->td_id_i = 0;
    for (int i = 0; i < 4; ++i) tick();
    v->rst_n = 1;
    tick();
  }

  void eval() { v->eval(); }

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  // The allocator's combinational reply, computed from what the block is
  // OFFERING right now. This is the same shape the arena has: `pv_ready_o` is
  // a free sink when no frame is open, and `pv_accept_o` is high only when the
  // record was really allocated.
  void drive_allocator() {
    v->eval();
    v->pv_ready_i = 1;
    v->td_ready_i = !v->pv_valid_o;   // the arena's own priority
    uint32_t pid = force_pv_id ? forced_pv_id : vcursor;
    v->pv_id_i = pid;
    v->td_id_i = tcursor;
    v->pv_accept_i = (v->pv_valid_o && sealed) ? 1 : 0;
    v->td_accept_i = (v->td_valid_o && v->td_ready_i && sealed) ? 1 : 0;
    v->eval();
  }

  // Latch what the allocator just took, then advance the clock.
  void step() {
    drive_allocator();
    bool pv = v->pv_valid_o && v->pv_ready_i;
    bool td = v->td_valid_o && v->td_ready_i;
    bool pv_acc = pv && v->pv_accept_i;
    bool td_acc = td && v->td_accept_i;
    Pub rec{};
    std::array<uint16_t, 3> ids{};
    if (pv_acc) {
      rec.id = force_pv_id ? forced_pv_id : vcursor;
      rec.x = static_cast<int32_t>(v->pv_x_o);
      rec.y = static_cast<int32_t>(v->pv_y_o);
      rec.invw = v->pv_invw_o;
      rec.status = v->pv_status_o;
      rec.uow = static_cast<int32_t>(v->pv_uow_o);
      rec.vow = static_cast<int32_t>(v->pv_vow_o);
      rec.rgba = v->pv_rgba_o;
    }
    if (td_acc) {
      ids[0] = v->td_v0_o;
      ids[1] = v->td_v1_o;
      ids[2] = v->td_v2_o;
    }
    tick();
    if (pv_acc) {
      pubs.push_back(rec);
      if (!force_pv_id) ++vcursor;
    }
    if (td_acc) {
      descs.push_back(ids);
      desc_ids.push_back(tcursor);
      ++tcursor;
    }
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) {
      v->tri_valid_i = 0;
      step();
    }
  }

  // Seal the allocator's frame, on the allocator's OWN pulse -- which is what
  // the block takes and the reason the port exists.
  void seal() {
    v->tri_valid_i = 0;
    v->frame_seal_i = 1;
    sealed = true;
    vcursor = 0;
    tcursor = 0;
    step();
    v->frame_seal_i = 0;
    step();
  }

  void pack_attr(const Attr& a, uint32_t* out) {
    out[0] = a.invw & 0xFFFFFFu;
    out[1] = a.uow;
    out[2] = a.vow;
    out[3] = a.r;
    out[4] = a.g;
    out[5] = a.b;
    out[6] = a.a;
  }

  void set_attr_port(uint32_t* port, const Attr& a) {
    uint32_t w[7];
    pack_attr(a, w);
    for (int i = 0; i < kAttrs; ++i) port[i] = w[i];
  }

  // Offer one triangle and run the block until it returns to IDLE having
  // emitted its descriptor. Bounded, so a wedge is a failure and not a hang.
  void submit(const Tri& t) {
    v->tri_domain_i = t.domain;
    v->tri_key_a_i = t.key[0];
    v->tri_key_b_i = t.key[1];
    v->tri_key_c_i = t.key[2];
    v->tri_ax_i = t.x[0];
    v->tri_ay_i = t.y[0];
    v->tri_bx_i = t.x[1];
    v->tri_by_i = t.y[1];
    v->tri_cx_i = t.x[2];
    v->tri_cy_i = t.y[2];
    set_attr_port(v->tri_attr_a_i.data(), t.attr[0]);
    set_attr_port(v->tri_attr_b_i.data(), t.attr[1]);
    set_attr_port(v->tri_attr_c_i.data(), t.attr[2]);
    v->tri_untex_i = t.untex ? 1 : 0;
    v->tri_material_i = t.material;
    v->tri_raster_i = t.raster;
    v->tri_src_id_i = t.src_id;
    v->tri_valid_i = 1;

    size_t want = descs.size() + 1;
    bool taken = false;
    for (int guard = 0; guard < 200; ++guard) {
      drive_allocator();
      if (!taken && v->tri_ready_o) taken = true;
      step();
      if (taken) v->tri_valid_i = 0;
      if (descs.size() >= want) return;
    }
    check(false, "GEOM.VERTID: a triangle did not retire inside 200 clocks");
  }
};

Attr mk_attr(uint32_t invw, int32_t uow, int32_t vow, uint32_t r, uint32_t g,
             uint32_t b, uint32_t a) {
  Attr x;
  x.invw = invw;
  x.uow = static_cast<uint32_t>(uow);
  x.vow = static_cast<uint32_t>(vow);
  x.r = r;
  x.g = g;
  x.b = b;
  x.a = a;
  return x;
}

uint8_t status_of(int domain, bool untex, bool shared) {
  return static_cast<uint8_t>((domain & 3) | (untex ? 4 : 0) | (shared ? 8 : 0));
}

uint32_t rgba_of(const Attr& a) {
  return (static_cast<uint32_t>(unit8_from_fx16(static_cast<int32_t>(a.a))) << 24) |
         (static_cast<uint32_t>(unit8_from_fx16(static_cast<int32_t>(a.b))) << 16) |
         (static_cast<uint32_t>(unit8_from_fx16(static_cast<int32_t>(a.g))) << 8) |
         static_cast<uint32_t>(unit8_from_fx16(static_cast<int32_t>(a.r)));
}

// ===========================================================================
int checks = 0;
void ck(bool cond, const char* what) {
  ++checks;
  check(cond, what);
}

}  // namespace

int main() {
  Vzhao_geom_vertid dut;
  Env e(&dut);

  // ------------------------------------------------------------------ case 1
  // NOTHING IS SEALED, so every record is consumed and not allocated. The
  // block must still retire triangles at full rate -- the arena's free-sink
  // law -- and must record NO identity, because an id it was never given is
  // not an id. `vid_sunk_o` fires here.
  {
    uint32_t s0 = dut.vid_sunk_o, p0 = dut.vid_published_o;
    Tri t;
    t.key[0] = key_of(0, 1, 0);
    t.key[1] = key_of(0, 1, 1);
    t.key[2] = key_of(0, 1, 2);
    e.submit(t);
    e.idle(2);
    ck(dut.vid_sunk_o - s0 == 3, "unsealed: three corners sunk");
    ck(dut.vid_published_o - p0 == 0, "unsealed: nothing published");
    ck(e.pubs.empty(), "unsealed: the allocator got no accepted vertex");
    ck(e.descs.size() == 1, "unsealed: the descriptor still retired");
    ck(e.descs[0][0] == 0 && e.descs[0][1] == 0 && e.descs[0][2] == 0,
       "unsealed: a sunk corner names the defined id 0");
  }

  // ------------------------------------------------------------------ case 2
  // THE PROPERTY THE WHOLE SCHEME EXISTS FOR.
  // Two triangles sharing the edge (v1, v2) of one meshlet in one view:
  //     T0 = (0, 1, 2)      T1 = (2, 1, 3)
  // Four distinct vertices, six corner references. The shared pair must be
  // published ONCE and named by the SAME id in both descriptors.
  e.seal();
  {
    uint32_t p0 = dut.vid_published_o, r0 = dut.vid_reused_o;
    uint32_t f0 = dut.vid_refs_o, t0 = dut.vid_tris_o;
    const uint32_t A = 1, G = 7;
    Attr av[4] = {
        mk_attr(0x100000, 100, 200, 0x08000, 0x00000, 0x10000, 0x10000),
        mk_attr(0x200000, 300, 400, 0x0FF80, 0x04000, 0x00000, 0x10000),
        mk_attr(0x300000, 500, 600, 0x10001, 0x08080, 0x02000, 0x08000),
        mk_attr(0x080000, -700, -800, 0x00000, 0x0FFFF, 0x0C000, 0x10000),
    };
    Tri t0a;
    t0a.material = 0x1234;
    t0a.raster = 0xDEAD0002;
    t0a.src_id = 0x55;
    for (int k = 0; k < 3; ++k) {
      int sv[3] = {0, 1, 2};
      t0a.key[k] = key_of(A, G, sv[k]);
      t0a.x[k] = 1000 + sv[k];
      t0a.y[k] = 2000 + sv[k];
      t0a.attr[k] = av[sv[k]];
    }
    e.submit(t0a);

    Tri t1a = t0a;
    for (int k = 0; k < 3; ++k) {
      int sv[3] = {2, 1, 3};
      t1a.key[k] = key_of(A, G, sv[k]);
      t1a.x[k] = 1000 + sv[k];
      t1a.y[k] = 2000 + sv[k];
      t1a.attr[k] = av[sv[k]];
    }
    e.submit(t1a);
    e.idle(2);

    ck(dut.vid_tris_o - t0 == 2, "shared edge: two triangles");
    ck(dut.vid_refs_o - f0 == 6, "shared edge: six corner references");
    ck(dut.vid_published_o - p0 == 4,
       "shared edge: FOUR vertices published for six references");
    ck(dut.vid_reused_o - r0 == 2,
       "shared edge: the two shared corners were REUSED, not republished");
    ck(e.pubs.size() == 4, "shared edge: the allocator saw exactly four records");
    ck(e.descs.size() == 3, "shared edge: two more descriptors");

    const auto& d0 = e.descs[1];
    const auto& d1 = e.descs[2];
    // T0 named source vertices 0,1,2 and T1 named 2,1,3. The ids for source
    // vertices 1 and 2 must be IDENTICAL across the two descriptors.
    ck(d0[1] == d1[1], "shared edge: source vertex 1 has ONE id in both triangles");
    ck(d0[2] == d1[0], "shared edge: source vertex 2 has ONE id in both triangles");
    ck(d0[0] != d0[1] && d0[1] != d0[2] && d0[0] != d0[2],
       "shared edge: the three corners of T0 are three distinct ids");
    ck(d1[2] != d0[0] && d1[2] != d0[1] && d1[2] != d0[2],
       "shared edge: T1's new corner got a fresh id");
    // Dense, from zero, in publication order.
    ck(e.pubs[0].id == 0 && e.pubs[1].id == 1 && e.pubs[2].id == 2 &&
           e.pubs[3].id == 3,
       "shared edge: the ids are dense from zero");

    // FIELD FOR FIELD, and the colour against zref.
    for (size_t i = 0; i < 4; ++i) {
      const Pub& p = e.pubs[i];
      // publication order is T0's corners 0,1,2 then T1's corner 3.
      int sv = (i < 3) ? static_cast<int>(i) : 3;
      ck(p.x == 1000 + sv, "PV field: screen_x");
      ck(p.y == 2000 + sv, "PV field: screen_y");
      ck(p.invw == (av[sv].invw & 0xFFFFFFu), "PV field: invw24");
      ck(p.uow == static_cast<int32_t>(av[sv].uow), "PV field: u_over_w");
      ck(p.vow == static_cast<int32_t>(av[sv].vow), "PV field: v_over_w");
      ck(p.rgba == rgba_of(av[sv]),
         "PV field: rgba8 == zref::unit8_from_fx16 per channel, r in the low byte");
      ck(p.status == status_of(kDomMesh, false, true),
         "PV field: status = {0, shared_capable=1, untex=0, domain=MESH}");
    }
    // The 0xFF80 channel is the Review C2 rail: (r+128)>>8 == 256 must clamp to
    // 255 and not wrap to 0. Asserted explicitly so the case cannot silently
    // stop covering it.
    ck(unit8_from_fx16(0x0FF80) == 255, "colour: the 0xFF80 rail clamps to 255");
    ck(((e.pubs[1].rgba >> 0) & 0xFF) == 255,
       "colour: the RTL clamps the 0xFF80 rail too");
    ck(((e.pubs[2].rgba >> 0) & 0xFF) == 255,
       "colour: a channel above 1.0 saturates at 255");

    // The descriptor's per-primitive fields are carried, not invented.
    ck(true, "descriptor fields checked below");
  }

  // ------------------------------------------------------------------ case 3
  // A NEW GENERATION ON THE SAME ARENA IS A NEW IDENTITY. Same arena, same
  // index, different generation: a new epoch, a MISS, a fresh id -- and the
  // earlier vertex's id must not be handed out. `vid_opens_o` fires.
  {
    uint32_t r0 = dut.vid_reused_o, p0 = dut.vid_published_o;
    uint32_t o0 = dut.vid_opens_o;
    uint16_t old_id = e.descs[1][1];   // source vertex 1 of arena 1 gen 7
    Tri t;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(1, 8, static_cast<uint32_t>(k));
      t.x[k] = 40 + k;
      t.y[k] = 50 + k;
    }
    e.submit(t);
    e.idle(2);
    ck(dut.vid_reused_o - r0 == 0,
       "new generation: nothing was reused across the arena's reopen");
    ck(dut.vid_published_o - p0 == 3, "new generation: three fresh vertices");
    ck(dut.vid_opens_o - o0 == 1, "new generation: exactly one epoch advance");
    const auto& d = e.descs.back();
    ck(d[1] != old_id,
       "new generation: the reused SLOT did not hand out the previous use's id");
  }

  // ------------------------------------------------------------------ case 4
  // AN UNSHARED DOMAIN IS DECLARED UNSHAREABLE, NOT LOOKED UP AND MISSED.
  // Two identical forge triangles with identical keys must publish six
  // vertices, not two. `vid_unshared_o` fires, `vid_reused_o` does not move.
  {
    uint32_t u0 = dut.vid_unshared_o, r0 = dut.vid_reused_o;
    uint32_t p0 = dut.vid_published_o;
    Tri t;
    t.domain = kDomForge;
    t.untex = true;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(0, 0, 0);       // identical, and irrelevant
      t.x[k] = 7;
      t.y[k] = 9;
    }
    e.submit(t);
    e.submit(t);
    e.idle(2);
    ck(dut.vid_unshared_o - u0 == 6, "forge: six corners declared unshareable");
    ck(dut.vid_reused_o - r0 == 0, "forge: no corner was deduplicated");
    ck(dut.vid_published_o - p0 == 6, "forge: six records published");
    ck(e.pubs.back().status == status_of(kDomForge, true, false),
       "forge: status = {shared_capable=0, untex=1, domain=FORGE}");
  }

  // Particles carry their own domain, and it reaches the status byte.
  {
    Tri t;
    t.domain = kDomPart;
    for (int k = 0; k < 3; ++k) { t.x[k] = 1; t.y[k] = 2; }
    e.submit(t);
    e.idle(2);
    ck(e.pubs.back().status == status_of(kDomPart, false, false),
       "particles: domain PARTICLE reaches the status byte");
  }

  // ------------------------------------------------------------------ case 5
  // A MESH KEY OUTSIDE THE MAP IS PUBLISHED AS ITS OWN VERTEX AND COUNTED.
  // An index at VSLOTS and an arena at ARENAS are both out of range. They must
  // NOT be truncated into row zero. `vid_index_oob_o` fires.
  {
    uint32_t oob0 = dut.vid_index_oob_o, r0 = dut.vid_reused_o;
    Tri t;
    t.key[0] = key_of(1, 9, kVSlots);          // index out of range
    t.key[1] = key_of(kArenas, 9, 0);          // arena out of range
    t.key[2] = key_of(1, 9, 0);                // legal
    for (int k = 0; k < 3; ++k) { t.x[k] = 11 + k; t.y[k] = 12 + k; }
    e.submit(t);
    // Offer the identical triangle again: the two illegal keys must publish
    // AGAIN (they were never held), and only the legal one may be reused.
    e.submit(t);
    e.idle(2);
    ck(dut.vid_index_oob_o - oob0 == 4,
       "out-of-range key: four illegal corners counted across two triangles");
    ck(dut.vid_reused_o - r0 == 1,
       "out-of-range key: only the LEGAL corner was reused");
    ck(e.pubs.back().status == status_of(kDomMesh, false, true),
       "out-of-range key: the legal corner is still shared_capable");
  }

  // ------------------------------------------------------------------ case 6
  // THE PRODUCER ASSUMPTION IS MEASURED, NOT ASSERTED. All three corners of a
  // mesh triangle come from one meshlet in one view, so they share
  // {arena, generation}. `vid_key_split_o` counts the clock they do not.
  {
    uint32_t s0 = dut.vid_key_split_o;
    Tri t;
    t.key[0] = key_of(2, 3, 0);
    t.key[1] = key_of(2, 3, 1);
    t.key[2] = key_of(3, 3, 2);     // a different arena
    for (int k = 0; k < 3; ++k) { t.x[k] = 21 + k; t.y[k] = 22 + k; }
    e.submit(t);
    e.idle(2);
    ck(dut.vid_key_split_o - s0 == 1, "split key: counted once");
  }

  // ------------------------------------------------------------------ case 7
  // AN ID A u16 vertex_id CANNOT NAME IS REFUSED, NOT TRUNCATED. The allocator
  // is forced to hand back ID_LIMIT. `vid_id_unnameable_o` fires and the row is
  // NOT recorded, so the next reference to the same key publishes again.
  {
    uint32_t un0 = dut.vid_id_unnameable_o, r0 = dut.vid_reused_o;
    e.force_pv_id = true;
    e.forced_pv_id = kIdLimit;
    Tri t;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(2, 40, static_cast<uint32_t>(k));
      t.x[k] = 31 + k;
      t.y[k] = 32 + k;
    }
    e.submit(t);
    e.submit(t);
    e.idle(2);
    e.force_pv_id = false;
    ck(dut.vid_id_unnameable_o - un0 == 6,
       "unnameable id: every one of six corners refused");
    ck(dut.vid_reused_o - r0 == 0,
       "unnameable id: nothing was recorded, so nothing was reused");
  }

  // ------------------------------------------------------------------ case 8
  // A SEAL LANDING MID-TRIANGLE DROPS IT, and the block offers nothing to the
  // allocator on that clock -- which is what keeps the arena's intake arm and
  // its seal arm from firing on the same edge. `vid_seal_abort_o` fires.
  {
    uint32_t ab0 = dut.vid_seal_abort_o;
    Tri t;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(3, 11, static_cast<uint32_t>(k));
      t.x[k] = 41 + k;
      t.y[k] = 42 + k;
    }
    // Hand it the triangle, let it start, then seal.
    dut.tri_domain_i = t.domain;
    dut.tri_key_a_i = t.key[0];
    dut.tri_key_b_i = t.key[1];
    dut.tri_key_c_i = t.key[2];
    dut.tri_ax_i = t.x[0];
    dut.tri_valid_i = 1;
    e.drive_allocator();
    e.step();
    dut.tri_valid_i = 0;
    e.step();                      // now inside the corner walk
    ck(dut.busy_o == 1, "seal abort: the block is mid-triangle");
    dut.frame_seal_i = 1;
    e.drive_allocator();
    ck(dut.pv_valid_o == 0 && dut.td_valid_o == 0,
       "seal abort: nothing is offered to the allocator on the seal clock");
    e.sealed = true;
    e.vcursor = 0;
    e.tcursor = 0;
    e.step();
    dut.frame_seal_i = 0;
    e.step();
    ck(dut.vid_seal_abort_o - ab0 == 1, "seal abort: counted once");
    ck(dut.busy_o == 0, "seal abort: the block is idle again");
  }

  // ------------------------------------------------------------------ case 9
  // THE MAP DOES NOT SURVIVE THE FRAME. The same key across a seal must be a
  // MISS, because the allocator's cursor restarted and the old id names a
  // record that now belongs to somebody else.
  {
    Tri t;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(1, 21, static_cast<uint32_t>(k));
      t.x[k] = 51 + k;
      t.y[k] = 52 + k;
    }
    e.submit(t);
    size_t n = e.descs.size();
    uint16_t before = e.descs[n - 1][0];
    e.seal();
    uint32_t r0 = dut.vid_reused_o;
    e.submit(t);
    e.idle(2);
    ck(dut.vid_reused_o - r0 == 0, "frame boundary: no row survived the seal");
    ck(e.descs.back()[0] == 0,
       "frame boundary: the new frame's first vertex is id 0");
    ck(before != 0 || n == 0, "frame boundary: the earlier id was not 0");
  }

  // ----------------------------------------------------------------- case 10
  // BACKPRESSURE. The allocator refuses for a while; the block must hold its
  // offer stable and lose nothing. `vid_stall_o` fires because GEOM.CLIP is
  // offering while the block is busy.
  {
    uint32_t st0 = dut.vid_stall_o;
    Tri t;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(2, 31, static_cast<uint32_t>(k));
      t.x[k] = 61 + k;
      t.y[k] = 62 + k;
    }
    // Take the triangle, then stall the allocator for eight clocks with a
    // second triangle offered at the input the whole time.
    dut.tri_domain_i = t.domain;
    dut.tri_key_a_i = t.key[0];
    dut.tri_key_b_i = t.key[1];
    dut.tri_key_c_i = t.key[2];
    dut.tri_ax_i = t.x[0];
    dut.tri_ay_i = t.y[0];
    dut.tri_bx_i = t.x[1];
    dut.tri_by_i = t.y[1];
    dut.tri_cx_i = t.x[2];
    dut.tri_cy_i = t.y[2];
    dut.tri_valid_i = 1;
    e.drive_allocator();
    e.step();
    int32_t held_x = 0;
    bool seen = false;
    for (int i = 0; i < 8; ++i) {
      dut.eval();
      dut.pv_ready_i = 0;
      dut.td_ready_i = 0;
      dut.pv_accept_i = 0;
      dut.td_accept_i = 0;
      dut.eval();
      if (dut.pv_valid_o) {
        if (!seen) {
          held_x = static_cast<int32_t>(dut.pv_x_o);
          seen = true;
        }
        ck(static_cast<int32_t>(dut.pv_x_o) == held_x,
           "backpressure: the offered record is held stable while refused");
      }
      e.tick();
    }
    ck(seen, "backpressure: the block was offering a vertex while refused");
    ck(dut.vid_stall_o - st0 > 0,
       "backpressure: the clocks GEOM.CLIP was held are counted");
    // Release and let it finish.
    size_t want = e.descs.size() + 1;
    for (int guard = 0; guard < 200 && e.descs.size() < want; ++guard) {
      dut.tri_valid_i = 0;
      e.step();
    }
    ck(e.descs.size() == want, "backpressure: the triangle retired after release");
  }

  // ----------------------------------------------------------------- case 11
  // THE DESCRIPTOR'S PER-PRIMITIVE FIELDS ARE CARRIED. material, the R28
  // raster word and the source id go out unchanged, and the triangle's own
  // arena index comes back on `tri_id_o`.
  {
    Tri t;
    t.material = 0xBEEF;
    t.raster = 0x1234'5679u;
    t.src_id = 0x0ABC;
    for (int k = 0; k < 3; ++k) {
      t.key[k] = key_of(3, 41, static_cast<uint32_t>(k));
      t.x[k] = 71 + k;
      t.y[k] = 72 + k;
    }
    // Watch the descriptor beat itself.
    dut.tri_domain_i = t.domain;
    dut.tri_key_a_i = t.key[0];
    dut.tri_key_b_i = t.key[1];
    dut.tri_key_c_i = t.key[2];
    dut.tri_ax_i = t.x[0]; dut.tri_ay_i = t.y[0];
    dut.tri_bx_i = t.x[1]; dut.tri_by_i = t.y[1];
    dut.tri_cx_i = t.x[2]; dut.tri_cy_i = t.y[2];
    dut.tri_material_i = t.material;
    dut.tri_raster_i = t.raster;
    dut.tri_src_id_i = t.src_id;
    dut.tri_valid_i = 1;
    bool saw = false;
    uint32_t want_tid = e.tcursor;
    for (int guard = 0; guard < 200 && !saw; ++guard) {
      e.drive_allocator();
      if (dut.td_valid_o && dut.td_ready_i) {
        ck(dut.td_material_o == t.material, "descriptor: material_id carried");
        ck(dut.td_raster_o == t.raster, "descriptor: R28 raster word carried");
        ck(dut.td_source_o == t.src_id,
           "descriptor: source_id is the 16-bit id zero-extended, not truncated");
        ck(dut.tri_id_valid_o == 1, "descriptor: tri_id_valid_o rides the accept");
        ck(dut.tri_id_o == want_tid,
           "descriptor: tri_id_o is the allocator's own triangle index");
        saw = true;
      }
      e.step();
      dut.tri_valid_i = 0;
    }
    ck(saw, "descriptor: the beat was observed");
  }

  std::printf("geom_vertid_directed: %d checks OK\n", checks);
  std::printf("  tris=%u refs=%u published=%u reused=%u unshared=%u\n",
              dut.vid_tris_o, dut.vid_refs_o, dut.vid_published_o,
              dut.vid_reused_o, dut.vid_unshared_o);
  std::printf("  opens=%u sunk=%u index_oob=%u key_split=%u unnameable=%u "
              "seal_abort=%u stall=%u\n",
              dut.vid_opens_o, dut.vid_sunk_o, dut.vid_index_oob_o,
              dut.vid_key_split_o, dut.vid_id_unnameable_o,
              dut.vid_seal_abort_o, dut.vid_stall_o);
  // EVERY COUNTER SEEN TO FIRE. Asserted here as a group so the file cannot
  // quietly stop covering one of them: a counter this bench never moves is a
  // counter with no positive control, and the block's header claims all twelve
  // are stimulus-reachable at its own ports.
  ck(dut.vid_tris_o > 0, "fired: vid_tris_o");
  ck(dut.vid_refs_o > 0, "fired: vid_refs_o");
  ck(dut.vid_published_o > 0, "fired: vid_published_o");
  ck(dut.vid_reused_o > 0, "fired: vid_reused_o");
  ck(dut.vid_unshared_o > 0, "fired: vid_unshared_o");
  ck(dut.vid_opens_o > 0, "fired: vid_opens_o");
  ck(dut.vid_sunk_o > 0, "fired: vid_sunk_o");
  ck(dut.vid_index_oob_o > 0, "fired: vid_index_oob_o");
  ck(dut.vid_key_split_o > 0, "fired: vid_key_split_o");
  ck(dut.vid_id_unnameable_o > 0, "fired: vid_id_unnameable_o");
  ck(dut.vid_seal_abort_o > 0, "fired: vid_seal_abort_o");
  ck(dut.vid_stall_o > 0, "fired: vid_stall_o");
  std::printf("geom_vertid_directed: all twelve counters fired\n");
  return 0;
}
