// twod_cmd_directed.cpp -- does the TWOD producer publish a SEALED frame, and
// can a later packet or the next frame mutate the list being consumed?
//
// ---------------------------------------------------------------------------
// THE LAW UNDER TEST, IN THE OWNER'S OWN WORDS
// ---------------------------------------------------------------------------
// Completion ruling 2026-09-22, item 3:
//
//   "Descriptors are frame-scoped. Stage and validate the frame's descriptors,
//    then publish a SEALED LIST at the boundary that owns that frame, before
//    its TWOD pass. NEITHER A LATER PACKET NOR THE NEXT FRAME MAY MUTATE THE
//    LIST BEING CONSUMED. Preserve deterministic order, define ties by command
//    order, and provide explicit plane disable behavior and an EMPTY-FRAME PATH
//    THAT CANNOT RETAIN OLD HUD CONTENTS."
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS AND IS NOT EVIDENCE ABOUT
// ---------------------------------------------------------------------------
// It drives this block's OWN ports, so states that are structurally unreachable
// in the composed console -- a stalled `d_ready_i`, two seals in consecutive
// cycles, a sixty-fifth descriptor -- are ordinary stimulus here. That is what
// lets the console assert those counters at zero and have the zero mean
// something.
//
// IT IS NOT the ruling's acceptance test. That is
// `twod_cmd_chain_directed.cpp`, which drives real packet bytes through
// CMD.DECODER and CMD.EXEC to composited pixels with the texels read from an
// arena -- "without descriptor or texel injection at a downstream boundary".
// This file injects descriptors at a downstream boundary ON PURPOSE, because
// that is the only way to reach the refusal states.
//
// ---------------------------------------------------------------------------
// THE CASE THAT MATTERS MOST IS CASE 6, AND IT IS NOT A DESCRIPTOR CASE
// ---------------------------------------------------------------------------
// R95: a counter must DISCRIMINATE, not merely move. `bind_conflict_o` fires
// when two descriptors whose binding slot agrees name different page regions --
// so case 6 first publishes two sprites that SHARE a region and requires ZERO,
// then changes one `base` by a single word and requires EXACTLY ONE. A detector
// that fires on everything is as useless as one that fires on nothing.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_twod_cmd.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kMaxDesc = 64;

Vzhao_twod_cmd* top = nullptr;

// What the publish walk handed out, in the order it handed it out. The ORDER of
// this vector is the evidence for the ruling's "preserve deterministic order,
// define ties by command order".
struct PubSprite {
  int      x, y, w, h;
  uint32_t u, v, a00, a01, a10, a11;
  int      fmt, pal, blend, vm, ord;
  uint16_t tint, src;
  int      bind_sel, bind_base, bind_lstride, bind_lheight;
};
struct PubPlane {
  int      slot, enable, role, blend, opacity, fmt, wrap_u, wrap_v, vm, pal;
  int      width, height;
  uint32_t a, b, c, d, u0, v0;
  int      bind_sel, bind_base;
};
std::vector<PubSprite> gSprites;
std::vector<PubPlane>  gPlanes;

int gSpriteStallEvery = 0;   // >0: s_ready_i low on 1 of N cycles
int gPlaneStallEvery  = 0;
long gCycles = 0;

void idleInputs() {
  top->pl_valid_i = 0;
  top->sp_valid_i = 0;
  top->pkt_commit_i = 0;
  top->pkt_abandon_i = 0;
  top->seal_i = 0;
  top->d_ready_i = 1;
  top->s_ready_i = 1;
}

// One clock, capturing whatever the publish walk emitted on it. The BINDING is
// captured beside the descriptor deliberately: a descriptor published with the
// wrong page region draws the wrong texels and every descriptor field still
// matches, which is exactly the shape this repository calls a silent defect.
void cyc() {
  top->d_ready_i = (gPlaneStallEvery  > 0) ? ((gCycles % gPlaneStallEvery)  == 0) : 1;
  top->s_ready_i = (gSpriteStallEvery > 0) ? ((gCycles % gSpriteStallEvery) == 0) : 1;
  top->eval();

  const bool dFire = top->d_valid_o && top->d_ready_i;
  const bool sFire = top->s_valid_o && top->s_ready_i;
  const bool bFire = top->ld_bind_we_o;

  PubPlane  pp{};
  PubSprite ps{};
  if (dFire) {
    pp.slot = top->d_slot_o;  pp.enable = top->d_enable_o;
    pp.role = top->d_role_o;  pp.blend  = top->d_blend_o;
    pp.opacity = top->d_opacity_o; pp.fmt = top->d_format_o;
    pp.wrap_u = top->d_wrap_u_o; pp.wrap_v = top->d_wrap_v_o;
    pp.vm = top->d_view_mask_o; pp.pal = top->d_palette_o;
    pp.width = top->d_width_o; pp.height = top->d_height_o;
    pp.a = top->d_a_o; pp.b = top->d_b_o; pp.c = top->d_c_o; pp.d = top->d_d_o;
    pp.u0 = top->d_u0_o; pp.v0 = top->d_v0_o;
    pp.bind_sel = bFire ? top->ld_bind_sel_o : -1;
    pp.bind_base = bFire ? top->ld_bind_base_o : -1;
  }
  if (sFire) {
    ps.x = static_cast<int16_t>(top->s_x_o);
    ps.y = static_cast<int16_t>(top->s_y_o);
    ps.w = top->s_w_o; ps.h = top->s_h_o;
    ps.u = top->s_u_o; ps.v = top->s_v_o;
    ps.a00 = top->s_a00_o; ps.a01 = top->s_a01_o;
    ps.a10 = top->s_a10_o; ps.a11 = top->s_a11_o;
    ps.fmt = top->s_format_o; ps.pal = top->s_palette_o;
    ps.blend = top->s_blend_o; ps.vm = top->s_view_mask_o;
    ps.ord = top->s_order_o;
    ps.tint = top->s_tint_o; ps.src = top->s_src_id_o;
    ps.bind_sel = bFire ? top->ld_bind_sel_o : -1;
    ps.bind_base = bFire ? top->ld_bind_base_o : -1;
    ps.bind_lstride = bFire ? top->ld_bind_lstride_o : -1;
    ps.bind_lheight = bFire ? top->ld_bind_lheight_o : -1;
  }

  top->clk = 1; top->eval();
  top->clk = 0; top->eval();
  ++gCycles;

  if (dFire) gPlanes.push_back(pp);
  if (sFire) gSprites.push_back(ps);
}

void hardReset() {
  delete top;
  top = new Vzhao_twod_cmd;
  gSprites.clear();
  gPlanes.clear();
  gSpriteStallEvery = gPlaneStallEvery = 0;
  idleInputs();
  top->rst_n = 0;
  top->clk = 0;
  for (int i = 0; i < 4; ++i) { top->eval(); top->clk = 1; top->eval(); top->clk = 0; top->eval(); }
  top->rst_n = 1;
  for (int i = 0; i < 2; ++i) cyc();
}

struct SpriteRec {
  int      x = 0, y = 0, w = 4, h = 4;
  int      base = 0, lstride = 2, lheight = 2;
  int      fmt = 1, pal = 0, blend = 0, vm = 1, ord = 0, flags = 0;
  uint16_t tint = 0xFFFF, src = 0;
  uint32_t u = 0, v = 0, a00 = 0x10000, a01 = 0, a10 = 0, a11 = 0x10000;
};

struct PlaneRec {
  int      slot = 0, role = 1, blend = 1, opacity = 255, fmt = 1;
  int      wrap = 0, vm = 1, pal = 0;
  int      width = 64, height = 64, flags = 1;
  int      base = 0, lstride = 6, lheight = 6;
  uint32_t a = 0x10000, b = 0, c = 0, d = 0x10000, u0 = 0, v0 = 0, ls = 0;
};

void pushSprite(const SpriteRec& r) {
  top->sp_valid_i = 1;
  top->sp_x_i = static_cast<int16_t>(r.x);
  top->sp_y_i = static_cast<int16_t>(r.y);
  top->sp_w_i = static_cast<uint16_t>(r.w);
  top->sp_h_i = static_cast<uint16_t>(r.h);
  top->sp_base_i = static_cast<uint16_t>(r.base);
  top->sp_lstride_i = static_cast<uint8_t>(r.lstride);
  top->sp_lheight_i = static_cast<uint8_t>(r.lheight);
  top->sp_format_i = static_cast<uint8_t>(r.fmt);
  top->sp_palette_i = static_cast<uint8_t>(r.pal);
  top->sp_blend_i = static_cast<uint8_t>(r.blend);
  top->sp_view_mask_i = static_cast<uint8_t>(r.vm);
  top->sp_tint_i = r.tint;
  top->sp_order_i = static_cast<uint8_t>(r.ord);
  top->sp_flags_i = static_cast<uint8_t>(r.flags);
  top->sp_src_id_i = r.src;
  top->sp_u_i = r.u; top->sp_v_i = r.v;
  top->sp_a00_i = r.a00; top->sp_a01_i = r.a01;
  top->sp_a10_i = r.a10; top->sp_a11_i = r.a11;
  cyc();
  top->sp_valid_i = 0;
}

void pushPlane(const PlaneRec& r) {
  top->pl_valid_i = 1;
  top->pl_slot_i = static_cast<uint8_t>(r.slot);
  top->pl_role_i = static_cast<uint8_t>(r.role);
  top->pl_blend_i = static_cast<uint8_t>(r.blend);
  top->pl_opacity_i = static_cast<uint8_t>(r.opacity);
  top->pl_format_i = static_cast<uint8_t>(r.fmt);
  top->pl_wrap_i = static_cast<uint8_t>(r.wrap);
  top->pl_view_mask_i = static_cast<uint8_t>(r.vm);
  top->pl_palette_i = static_cast<uint8_t>(r.pal);
  top->pl_width_i = static_cast<uint16_t>(r.width);
  top->pl_height_i = static_cast<uint16_t>(r.height);
  top->pl_flags_i = static_cast<uint16_t>(r.flags);
  top->pl_base_i = static_cast<uint16_t>(r.base);
  top->pl_lstride_i = static_cast<uint8_t>(r.lstride);
  top->pl_lheight_i = static_cast<uint8_t>(r.lheight);
  top->pl_a_i = r.a; top->pl_b_i = r.b; top->pl_c_i = r.c; top->pl_d_i = r.d;
  top->pl_u0_i = r.u0; top->pl_v0_i = r.v0;
  top->pl_line_scroll_i = r.ls;
  cyc();
  top->pl_valid_i = 0;
}

void commit()  { top->pkt_commit_i = 1; cyc(); top->pkt_commit_i = 0; }
void abandon() { top->pkt_abandon_i = 1; cyc(); top->pkt_abandon_i = 0; }

// The seal, then long enough for the WHOLE walk: pass 1, the two disables and
// pass 2. Bounded at 4*MAX_DESC + 6 by the contract, so 300 is generous and
// finite -- a `while (busy)` would hang on the defect it is meant to catch.
void sealAndDrain(int extra = 300) {
  top->seal_i = 1; cyc(); top->seal_i = 0;
  for (int i = 0; i < extra; ++i) cyc();
}

// The repository's checker takes (cond, what, expected, actual); this wrapper
// keeps the call sites reading as `what, got, want` and hands them over in the
// order zhao_sim.hpp wants. It exists so a transposed pair cannot hide in a
// three-hundred-line file.
void ckEq(const char* what, uint64_t got, uint64_t want) {
  zhao::check(got == want, what, want, got);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // =========================================================================
  // 1. ONE SPRITE AND ONE PLANE, STAGED, COMMITTED, PUBLISHED
  // =========================================================================
  // The baseline, and it asserts the FIELDS rather than the count: a producer
  // that publishes the right number of descriptors with the wrong contents is
  // the defect no counter can see.
  {
    hardReset();
    PlaneRec p; p.slot = 1; p.role = 1; p.blend = 2; p.opacity = 0x40;
    p.width = 128; p.height = 96; p.pal = 3; p.base = 512; p.lstride = 7;
    p.lheight = 6; p.u0 = 0x00050000; p.v0 = 0x00060000; p.ls = 0x00020000;
    pushPlane(p);

    SpriteRec s; s.x = 17; s.y = -3; s.w = 12; s.h = 5; s.ord = 200;
    s.src = 0x1234; s.tint = 0xBEEF; s.pal = 2; s.base = 64; s.lstride = 3;
    s.lheight = 3; s.u = 0x00010000; s.v = 0x00020000;
    pushSprite(s);

    commit();
    sealAndDrain();

    ckEq("1: one plane published", gPlanes.size(), 2u);   // slot 1 + slot 0 auto-disable
    ckEq("1: one sprite published", gSprites.size(), 1u);
    ckEq("1: frames sealed", top->frames_sealed_o, 1u);
    ckEq("1: planes staged", top->planes_staged_o, 1u);
    ckEq("1: sprites staged", top->sprites_staged_o, 1u);
    ckEq("1: planes published", top->planes_published_o, 1u);
    ckEq("1: sprites published", top->sprites_published_o, 1u);
    ckEq("1: one slot auto-disabled", top->slots_auto_disabled_o, 1u);
    ckEq("1: nothing refused", top->plane_refused_o + top->sprite_refused_o, 0u);
    ckEq("1: no overflow", top->list_overflow_o, 0u);
    ckEq("1: no bind conflict", top->bind_conflict_o, 0u);
    ckEq("1: no seal overrun", top->seal_overrun_o, 0u);

    const PubPlane& P = gPlanes[0];
    ckEq("1: plane slot", P.slot, 1);
    ckEq("1: plane enabled", P.enable, 1);
    ckEq("1: plane role", P.role, 1);
    ckEq("1: plane blend", P.blend, 2);
    ckEq("1: plane opacity", P.opacity, 0x40);
    ckEq("1: plane width", P.width, 128);
    ckEq("1: plane height", P.height, 96);
    ckEq("1: plane palette", P.pal, 3);
    ckEq("1: plane u0", P.u0, 0x00050000u);
    ckEq("1: plane v0", P.v0, 0x00060000u);
    // ROLE selects the binding slot, which is the sampler's own convention.
    ckEq("1: plane binding slot is its ROLE", P.bind_sel, 1);
    ckEq("1: plane binding base", P.bind_base, 512);
    ckEq("1: atm slot follows the ATMOSPHERE plane", (int)top->atm_slot_o, 1);
    ckEq("1: line scroll is that record's own", (uint32_t)top->line_scroll_o, 0x00020000u);

    ckEq("1: the auto-disable is slot 0", gPlanes[1].slot, 0);
    ckEq("1: the auto-disable is a DISABLE", gPlanes[1].enable, 0);

    const PubSprite& S = gSprites[0];
    ckEq("1: sprite x", S.x, 17);
    ckEq("1: sprite y", S.y, -3);
    ckEq("1: sprite w", S.w, 12);
    ckEq("1: sprite h", S.h, 5);
    ckEq("1: sprite order", S.ord, 200);
    ckEq("1: sprite src", (int)S.src, 0x1234);
    ckEq("1: sprite tint carried", (int)S.tint, 0xBEEF);
    ckEq("1: sprite u", S.u, 0x00010000u);
    ckEq("1: sprite v", S.v, 0x00020000u);
    // 4 + src_id[1:0]: 0x1234 & 3 == 0, so slot 4.
    ckEq("1: sprite binding slot is 4 + src[1:0]", S.bind_sel, 4);
    ckEq("1: sprite binding base", S.bind_base, 64);
    ckEq("1: sprite binding lstride", S.bind_lstride, 3);
  }

  // =========================================================================
  // 2. AN ABANDONED PACKET PUBLISHES NOTHING
  // =========================================================================
  // The atomicity case. The same records, the same seal -- only the verdict
  // differs, and that is the whole of it.
  {
    hardReset();
    SpriteRec s; s.src = 7;
    pushSprite(s);
    abandon();
    sealAndDrain();

    ckEq("2: sprite staged", top->sprites_staged_o, 1u);
    ckEq("2: packet abandoned", top->packets_abandoned_o, 1u);
    ckEq("2: NOTHING published", top->sprites_published_o, 0u);
    ckEq("2: the frame still sealed", top->frames_sealed_o, 1u);
    ckEq("2: both slots auto-disabled", top->slots_auto_disabled_o, 2u);
    ckEq("2: no sprite reached the list", gSprites.size(), 0u);

    // ...and the SAME records, committed by a later packet, DO publish. That
    // is what makes the line above a rollback rather than a drop.
    pushSprite(s);
    commit();
    sealAndDrain();
    ckEq("2: the next packet's copy publishes", top->sprites_published_o, 1u);
  }

  // =========================================================================
  // 3. THE EMPTY FRAME
  // =========================================================================
  // The ruling: "an empty-frame path that cannot retain old HUD contents."
  // A frame carrying no records publishes two disables and an empty list.
  {
    hardReset();
    SpriteRec s; s.src = 1;
    pushSprite(s);
    PlaneRec p; p.slot = 0; p.role = 0; p.blend = 0;
    pushPlane(p);
    commit();
    sealAndDrain();
    const size_t after_first_sprites = gSprites.size();
    const size_t after_first_planes  = gPlanes.size();
    ckEq("3: the first frame drew", after_first_sprites, 1u);

    // Frame two: no records, no packet.
    sealAndDrain();
    ckEq("3: the empty frame publishes NO sprite",
          gSprites.size() - after_first_sprites, 0u);
    ckEq("3: the empty frame publishes TWO disables",
          gPlanes.size() - after_first_planes, 2u);
    ckEq("3: both are disables",
          (gPlanes[after_first_planes].enable == 0) &&
          (gPlanes[after_first_planes + 1].enable == 0), true);
    ckEq("3: the two slots are 0 and 1",
          gPlanes[after_first_planes].slot + gPlanes[after_first_planes + 1].slot, 1);
    ckEq("3: no refusal for an empty frame",
          top->plane_refused_o + top->sprite_refused_o + top->list_overflow_o, 0u);
  }

  // =========================================================================
  // 4. ORDER IS COMMAND ORDER, AND A TIE IS BROKEN BY IT
  // =========================================================================
  // The ruling: "preserve deterministic order, define ties by command order."
  // Three sprites with the SAME `order` must come out in the order they went
  // in -- which is the only thing the band's last-write composition can use.
  {
    hardReset();
    for (int i = 0; i < 3; ++i) {
      SpriteRec s; s.src = static_cast<uint16_t>(0x100 + i); s.ord = 42;
      s.x = i * 10;
      pushSprite(s);
    }
    commit();
    sealAndDrain();
    ckEq("4: three published", gSprites.size(), 3u);
    ckEq("4: first is first",  (int)gSprites[0].src, 0x100);
    ckEq("4: second is second", (int)gSprites[1].src, 0x101);
    ckEq("4: third is third",  (int)gSprites[2].src, 0x102);
  }

  // =========================================================================
  // 5. THE FRAME BUDGET DROPS THE TAIL AND COUNTS IT
  // =========================================================================
  // TWOD.SPRITE.md: "drop the tail, deterministically by `order`, and count --
  // THE HUD MUST NOT FAULT A FRAME." So the first MAX_DESC still publish, and
  // that half is what makes it a tail drop rather than a failure.
  {
    hardReset();
    for (int i = 0; i < kMaxDesc + 3; ++i) {
      SpriteRec s; s.src = static_cast<uint16_t>(i); s.ord = static_cast<uint8_t>(i);
      pushSprite(s);
    }
    commit();
    sealAndDrain(600);
    ckEq("5: exactly MAX_DESC staged", top->sprites_staged_o, (uint32_t)kMaxDesc);
    ckEq("5: three dropped and counted", top->list_overflow_o, 3u);
    ckEq("5: exactly MAX_DESC published", top->sprites_published_o, (uint32_t)kMaxDesc);
    ckEq("5: the list is MAX_DESC long", gSprites.size(), (size_t)kMaxDesc);
    ckEq("5: the HEAD survived", (int)gSprites[0].src, 0);
    ckEq("5: the TAIL is what went", (int)gSprites[kMaxDesc - 1].src, kMaxDesc - 1);
  }

  // =========================================================================
  // 6. bind_conflict_o DISCRIMINATES (R95)
  // =========================================================================
  {
    // (a) two sprites sharing a binding slot AND a region: SILENT.
    hardReset();
    { SpriteRec s; s.src = 4; s.base = 128; pushSprite(s); }
    { SpriteRec s; s.src = 8; s.base = 128; pushSprite(s); }   // 4 & 3 == 8 & 3 == 0
    commit();
    sealAndDrain();
    ckEq("6a: same slot, same region -> no conflict", top->bind_conflict_o, 0u);
    ckEq("6a: both published", top->sprites_published_o, 2u);

    // (b) one word apart: EXACTLY ONE.
    hardReset();
    { SpriteRec s; s.src = 4; s.base = 128; pushSprite(s); }
    { SpriteRec s; s.src = 8; s.base = 129; pushSprite(s); }
    commit();
    sealAndDrain();
    ckEq("6b: same slot, different region -> exactly one", top->bind_conflict_o, 1u);

    // (c) different slots, different regions: SILENT. The detector must key on
    // the SLOT and not on the region changing.
    hardReset();
    { SpriteRec s; s.src = 4; s.base = 128; pushSprite(s); }
    { SpriteRec s; s.src = 5; s.base = 900; pushSprite(s); }   // slot 4 vs 5
    commit();
    sealAndDrain();
    ckEq("6c: different slots -> no conflict", top->bind_conflict_o, 0u);
  }

  // =========================================================================
  // 7. EVERY REFUSAL CLAUSE, ONE AT A TIME, ON ITS OWN COUNTER
  // =========================================================================
  // What is refused HERE is only what cannot be REPRESENTED downstream. A
  // reserved role, a BACKDROP asking for ALPHA and a zero-extent sprite are
  // NOT here: the consumers own those and the ruling requires they be retained.
  {
    struct Bad { const char* what; PlaneRec p; };
    const Bad bads[] = {
      {"slot > 1",              [] { PlaneRec p; p.slot = 2; return p; }()},
      {"role > 3",              [] { PlaneRec p; p.role = 4; return p; }()},
      {"blend > 3",             [] { PlaneRec p; p.blend = 7; return p; }()},
      {"format > 1",            [] { PlaneRec p; p.fmt = 2; return p; }()},
      {"wrap reserved bit",     [] { PlaneRec p; p.wrap = 4; return p; }()},
      {"view_mask reserved bit",[] { PlaneRec p; p.vm = 8; return p; }()},
      {"flags reserved bit",    [] { PlaneRec p; p.flags = 3; return p; }()},
      {"base past the store",   [] { PlaneRec p; p.base = 8192; return p; }()},
      {"lstride > 15",          [] { PlaneRec p; p.lstride = 16; return p; }()},
      {"lheight > 15",          [] { PlaneRec p; p.lheight = 16; return p; }()},
    };
    for (const auto& b : bads) {
      hardReset();
      pushPlane(b.p);
      commit();
      sealAndDrain();
      char msg[128];
      std::snprintf(msg, sizeof msg, "7: plane refused: %s", b.what);
      ckEq(msg, top->plane_refused_o, 1u);
      std::snprintf(msg, sizeof msg, "7: nothing staged: %s", b.what);
      ckEq(msg, top->planes_staged_o, 0u);
      std::snprintf(msg, sizeof msg, "7: nothing published: %s", b.what);
      ckEq(msg, top->planes_published_o, 0u);
      // And the frame still completes with both slots off -- a refused plane
      // draws nothing and the frame is not faulted.
      std::snprintf(msg, sizeof msg, "7: both slots off: %s", b.what);
      ckEq(msg, top->slots_auto_disabled_o, 2u);
    }

    struct BadS { const char* what; SpriteRec s; };
    const BadS badss[] = {
      {"format > 7",            [] { SpriteRec s; s.fmt = 8; return s; }()},
      {"blend reserved bit",    [] { SpriteRec s; s.blend = 4; return s; }()},
      {"view_mask reserved bit",[] { SpriteRec s; s.vm = 8; return s; }()},
      {"flags nonzero",         [] { SpriteRec s; s.flags = 1; return s; }()},
      {"base past the store",   [] { SpriteRec s; s.base = 8192; return s; }()},
      {"lstride > 15",          [] { SpriteRec s; s.lstride = 16; return s; }()},
      {"lheight > 15",          [] { SpriteRec s; s.lheight = 16; return s; }()},
    };
    for (const auto& b : badss) {
      hardReset();
      pushSprite(b.s);
      commit();
      sealAndDrain();
      char msg[128];
      std::snprintf(msg, sizeof msg, "7: sprite refused: %s", b.what);
      ckEq(msg, top->sprite_refused_o, 1u);
      std::snprintf(msg, sizeof msg, "7: nothing staged: %s", b.what);
      ckEq(msg, top->sprites_staged_o, 0u);
      std::snprintf(msg, sizeof msg, "7: no list overflow: %s", b.what);
      ckEq(msg, top->list_overflow_o, 0u);
    }

    // ...AND THE NEGATIVE HALF. A sprite whose width is ZERO is legal HERE and
    // refused by `zhao_twod_sprite`, which is the rule the ruling says to keep.
    hardReset();
    { SpriteRec s; s.w = 0; pushSprite(s); }
    commit();
    sealAndDrain();
    ckEq("7: a zero-width sprite is NOT this block's refusal",
          top->sprite_refused_o, 0u);
    ckEq("7: it is published, for TWOD.SPRITE to refuse",
          top->sprites_published_o, 1u);
  }

  // =========================================================================
  // 8. A STALLED CONSUMER DOES NOT DUPLICATE A DESCRIPTOR
  // =========================================================================
  // The emit state and the wait state are split for exactly this: an emit state
  // that also judged the handshake would re-emit the held entry every stalled
  // cycle, and `sprites_published_o` would report a HUD several times the size
  // of the one on screen. CLAUDE.md: "a test that checks WHAT came out cannot
  // see HOW MANY TIMES the machine did it."
  {
    hardReset();
    for (int i = 0; i < 5; ++i) { SpriteRec s; s.src = static_cast<uint16_t>(i); pushSprite(s); }
    PlaneRec p; p.slot = 0; pushPlane(p);
    commit();
    gSpriteStallEvery = 7;   // s_ready_i high 1 cycle in 7
    gPlaneStallEvery  = 5;
    sealAndDrain(900);
    gSpriteStallEvery = gPlaneStallEvery = 0;
    ckEq("8: exactly five published under a stall", top->sprites_published_o, 5u);
    ckEq("8: the list is five long", gSprites.size(), 5u);
    ckEq("8: still in command order",
          (gSprites[0].src == 0) && (gSprites[4].src == 4), true);
    ckEq("8: one plane and one disable", top->planes_published_o, 1u);
    ckEq("8: exactly one auto-disable", top->slots_auto_disabled_o, 1u);
  }

  // =========================================================================
  // 9. A PACKET IN FLIGHT AT THE SEAL IS NOT SPLIT
  // =========================================================================
  // Its staged records sit beyond the sealed window and commit into the NEXT
  // frame WHOLE. The ping-pong design this replaced had to discard them.
  {
    hardReset();
    { SpriteRec s; s.src = 0xA0; pushSprite(s); }
    commit();                                   // frame 1's packet
    { SpriteRec s; s.src = 0xB0; pushSprite(s); }  // frame 2's packet, in flight
    { SpriteRec s; s.src = 0xB1; pushSprite(s); }
    sealAndDrain();                             // the seal lands mid-packet
    ckEq("9: frame 1 published exactly its own", gSprites.size(), 1u);
    ckEq("9: and it is the right one", (int)gSprites[0].src, 0xA0);

    { SpriteRec s; s.src = 0xB2; pushSprite(s); }
    commit();                                   // the in-flight packet lands
    sealAndDrain();
    ckEq("9: frame 2 published all THREE of the split packet", gSprites.size(), 4u);
    ckEq("9: in order", (int)gSprites[1].src, 0xB0);
    ckEq("9: in order", (int)gSprites[2].src, 0xB1);
    ckEq("9: in order", (int)gSprites[3].src, 0xB2);
  }

  // =========================================================================
  // 10. seal_overrun_o FIRES, AND DISCRIMINATES
  // =========================================================================
  {
    hardReset();
    { SpriteRec s; s.src = 1; pushSprite(s); }
    commit();
    // Two seals, the second while the walk is running. `seal_i` is a PULSE and
    // is cleared between them on purpose: leaving it high for three cycles
    // counts three seals, which is correct behaviour and a wrong test -- the
    // first version of this case did exactly that and read 2.
    top->seal_i = 1; cyc(); top->seal_i = 0;
    cyc();
    top->seal_i = 1; cyc(); top->seal_i = 0;
    for (int i = 0; i < 300; ++i) cyc();
    ckEq("10: the second seal is counted", top->seal_overrun_o, 1u);
    ckEq("10: one frame sealed, not two", top->frames_sealed_o, 1u);

    // The negative half: two seals with the walk finished between them are two
    // frames and NO overrun.
    hardReset();
    { SpriteRec s; s.src = 1; pushSprite(s); }
    commit();
    sealAndDrain();
    sealAndDrain();
    ckEq("10: two clean seals are two frames", top->frames_sealed_o, 2u);
    ckEq("10: and no overrun", top->seal_overrun_o, 0u);
  }

  // =========================================================================
  // 11. THE PLANE PAIR IS PUBLISHED BEFORE THE SPRITES
  // =========================================================================
  // Not a preference: the sampler starts prefilling atmosphere lines at the
  // frame tick, so a SetPlane sitting at the END of a full list must still
  // reach TWOD.PLANE inside the first line. Pass 1 is what buys that, and this
  // measures the clock it lands on.
  {
    hardReset();
    for (int i = 0; i < kMaxDesc - 1; ++i) {
      SpriteRec s; s.src = static_cast<uint16_t>(i); pushSprite(s);
    }
    PlaneRec p; p.slot = 0; p.role = 1; p.blend = 1;
    pushPlane(p);                                  // the LAST entry in the list
    commit();

    const long t0 = gCycles;
    top->seal_i = 1; cyc(); top->seal_i = 0;
    long tPlane = -1, tFirstSprite = -1;
    for (int i = 0; i < 900; ++i) {
      const size_t np = gPlanes.size(), ns = gSprites.size();
      cyc();
      if (tPlane < 0 && gPlanes.size() > np) tPlane = gCycles - t0;
      if (tFirstSprite < 0 && gSprites.size() > ns) tFirstSprite = gCycles - t0;
    }
    ckEq("11: the plane published", gPlanes.size() >= 1u, true);
    ckEq("11: the sprites published", gSprites.size(), (size_t)(kMaxDesc - 1));
    ckEq("11: the plane came FIRST", (tPlane >= 0) && (tPlane < tFirstSprite), true);
    // The contract's bound: 2*MAX_DESC + 4 = 132 clocks at MAX_DESC = 64,
    // against 384 for the sampler's first line. Asserted as a NUMBER so the
    // margin is measured rather than argued.
    ckEq("11: inside the contract's 2*MAX_DESC + 4", tPlane <= 2 * kMaxDesc + 4, true);
    std::printf("  [11] plane published at clock %ld after the seal, "
                "first sprite at %ld (bound %d, sampler's first line 384)\n",
                tPlane, tFirstSprite, 2 * kMaxDesc + 4);
  }

  delete top;
  return zhao::report_and_exit("twod_cmd_directed");
}
