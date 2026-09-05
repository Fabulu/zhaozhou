// MANAFOLD (creature 02) — creature 02, the floating mana-conduit companion. COMPOSITION.
//
// The only header zhao_reel.cpp includes for this creature. It must sit
// after `namespace zc = zref::creature;`, the same contract zixxtrixx.h
// uses. The headers are split for the ownership migration: a later move
// into Upheaval/creature/Manafold/source/ is a git mv, not a rewrite.
//
//   manafold_art.h    every named knob
//   manafold_model.h  make_ball(), the body, the hinges (the loop + eyes
//                      land at the form milestone)
//   manafold_rig.h    bone ids + skeleton
//   manafold_clips.h  maths helpers (sanctioned copy) + clip builders
//
// The creature FLOATS (OWNER-DIRECTION-1): no gait, no attacks, no ground
// contact — the probe asserts clearance, and Zixxtrixx's ground-contact law
// is declared not applicable in CREATURE.json.

#ifndef ZHAO_REEL_MANAFOLD_H
#define ZHAO_REEL_MANAFOLD_H

#include <cstdlib>
#include <string>

#include "manafold_art.h"
#include "manafold_model.h"
#include "manafold_rig.h"
#include "manafold_clips.h"
#include "manafold_fx.h"
// LANE-ONLY (Owner Direction 6, the experimental mana reel): the variant
// table, the forked fold and the lab clip. Ships nothing; touches no
// shipped constant. Delete this include and the push_back below to revert.
#include "manafold_lab.h"

// The GENERATED page (tools/pack/mkmanafoldpage.py) is COMMITTED, exactly as
// Zixxtrixx's pages are: the generator is deterministic, so the tracked
// header is byte-stable — regenerate and diff after editing the generator.
// The include is deliberately UNGUARDED. There is no usable fallback:
// pageless parts do not render grey, they render BLACK under celmain
// (09-ENGINE-GOTCHAS.md §0/§7), and a __has_include guard here once let a
// clean checkout build silently and ship a solid-black creature (pass-4
// review). A missing page must fail the build, loudly — build-direct.sh
// checks for the file before linking, matching the zixxtrixx_page_cel.h
// precedent.
#include "manafold_page.h"
#define U02_HAVE_PAGE 1

namespace u02 {

#ifdef U02_HAVE_PAGE
// Direct-colour page set: page 0 = the 256x256 atlas (body/loop/hinge row
// bands selected per part via v0/v1), page 1 = the separate 64x64 eye tile
// (bilinear + mips bleed across atlas neighbours; the lens shares nothing),
// page 2 = the 64x64 flat-cyan pupil-star tile (the star must be textured:
// untextured parts render black under celmain — 09-ENGINE-GOTCHAS.md §7).
inline const zref::DirectPageSet& page_direct() {
  static const zref::DirectPageSet ps = [] {
    zref::DirectPageSet p;
    p.mem.base = 0;
    const uint32_t atlas_bytes = static_cast<uint32_t>(kU02AtlasWords) * 2;
    const uint32_t eye_bytes = static_cast<uint32_t>(kU02EyeWords) * 2;
    const uint32_t star_bytes = static_cast<uint32_t>(kU02StarWords) * 2;
    p.mem.bytes.resize(atlas_bytes + eye_bytes + star_bytes);
    for (int i = 0; i < kU02AtlasWords; ++i) {
      p.mem.bytes[static_cast<size_t>(i) * 2] = static_cast<uint8_t>(kU02Atlas[i] & 0xFF);
      p.mem.bytes[static_cast<size_t>(i) * 2 + 1] = static_cast<uint8_t>(kU02Atlas[i] >> 8);
    }
    for (int i = 0; i < kU02EyeWords; ++i) {
      p.mem.bytes[atlas_bytes + static_cast<size_t>(i) * 2] =
          static_cast<uint8_t>(kU02Eye[i] & 0xFF);
      p.mem.bytes[atlas_bytes + static_cast<size_t>(i) * 2 + 1] =
          static_cast<uint8_t>(kU02Eye[i] >> 8);
    }
    for (int i = 0; i < kU02StarWords; ++i) {
      p.mem.bytes[atlas_bytes + eye_bytes + static_cast<size_t>(i) * 2] =
          static_cast<uint8_t>(kU02Star[i] & 0xFF);
      p.mem.bytes[atlas_bytes + eye_bytes + static_cast<size_t>(i) * 2 + 1] =
          static_cast<uint8_t>(kU02Star[i] >> 8);
    }
    zref::Tmu::Mode ma;  // the atlas
    ma.fmt = zref::Tmu::kRgb565;
    ma.bilinear = true;
    ma.wrap_u = zref::Tmu::kRepeat;  // seamless around every ring
    ma.wrap_v = zref::Tmu::kClamp;   // row bands must not bleed across parts
    ma.log2w = 8;
    ma.log2h = 8;
    ma.max_level = 7;
    ma.mip_en = true;
    zref::Tmu::Mode me = ma;  // the eye page
    me.log2w = 6;
    me.log2h = 6;
    me.max_level = 6;
    const zref::Tmu::Mode ms = me;  // the star page: same 64x64 shape
    p.mode = ma.pack();
    p.tile_base = {0, atlas_bytes, atlas_bytes + eye_bytes};
    p.tile_mode = {ma.pack(), me.pack(), ms.pack()};
    return p;
  }();
  return ps;
}
#endif

inline const zc::CreatureType& type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk = build_skeleton();

    std::vector<zc::RingPart> parts;
    parts.push_back(make_body(kBRoot));
    // PASS 6 B.2 (Direction 5 §2b): the three hinge balls and both junction
    // knuckles are GONE. make_loop() now carries their swell in its own skin,
    // so the antenna is ONE continuous surface from the body to the re-entry.
    parts.push_back(make_loop());
    // PASS 6 B.1 (Direction 5 §5/§5a/§5b): the eye is exactly TWO things that
    // move -- the STAR (white outer + cyan inner, one rigid unit on the pupil
    // bone) and the PURPLE (the lens on the eye bone). Not a star, a ring, a
    // highlight and a pupil kept in agreement by hand.
    //
    // The white is pushed FIRST so the cyan draws over it, and both ride the
    // same pupil bone: no pose can slide one against the other. The U02_EYE
    // A/B branch is retired -- the owner has decided.
    parts.push_back(make_eye_lens(kBEyeL));
    parts.push_back(make_eye_lens(kBEyeR));
    parts.push_back(make_star(kBPupilL, true));   // white outer star
    parts.push_back(make_star(kBPupilR, true));
    parts.push_back(make_star(kBPupilL, false));  // cyan inner star
    parts.push_back(make_star(kBPupilR, false));
    // Hinge parts are rigid on their own bones whose bind translation IS the
    // ball centre; the loop is one straight-bound chain the fold pose bends.

    zc::ClipBank bank;
    bank.bone_count = kBoneCount;
    bank.clips.push_back(build_hover_idle());  // slot 0
    bank.clips.push_back(build_drift());       // slot 1
    bank.clips.push_back(build_channel());     // slot 2
    bank.clips.push_back(build_curious());     // slot 3
    bank.clips.push_back(build_startle());     // slot 4
    bank.clips.push_back(build_rest());        // slot 5
    bank.clips.push_back(build_pirouette());   // slot 6
    bank.clips.push_back(build_still());       // slot 7 (form diagnostics)
    bank.clips.push_back(build_hasty());       // slot 8 (Direction 2 §5)
    bank.clips.push_back(build_fall());        // slot 9
    bank.clips.push_back(build_hit());         // slot 10
    bank.clips.push_back(build_taunt());       // slot 11
    bank.clips.push_back(build_taunt2());      // slot 12
    bank.clips.push_back(build_trick());       // slot 13 (pass 3: headstand)
    bank.clips.push_back(build_damage());      // slot 14 (pass 4: directional hits)
    bank.clips.push_back(lab::build_manalab());  // slot 15 (LANE-ONLY: Direction 6)

    zc::CreatureType type;
    type.type_id = 3;  // 1 watchdog, 2 zixxtrixx, 3 manafold
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "manafold: compile failed: %s\n", reason);
    }
#ifdef U02_HAVE_PAGE
    type.page_direct = &page_direct();
#endif
    return type;
  }();
  return t;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_H
