// zref_surface.hpp — the SURFACE.SHEET / SURFACE.STAMP oracle.
//
// THREE LAYERS, AND THEY ARE NOT EQUALLY RATIFIED. Read the labels.
//
//   (1) FOUND, ratified, already shipping. The analytic circle/annulus stamp
//       into the 64x64 {tag u8, strength u8} sheet is
//       `zref::render::stamp_surface` (reference/src/zrender/terrain.cpp),
//       executed by the software console for the ABI opcode SurfaceStamp
//       0x0210 and pinned by committed goldens (tests/render/render_golden.cpp
//       stamps a crack ring; tests/render/render_heightfield.cpp checks the
//       annulus hole texel by texel). This header does NOT reimplement it: the
//       per-texel decomposition below is a VIEW, and
//       tests/surface/surface_stamp_directed.cpp proves the view is faithful
//       by running randomized commands through `stamp_surface` and through
//       this decomposition and requiring all 4,096 texels to agree bit for
//       bit. Without that cross-check this file would be a second
//       implementation, which charter 29-6 forbids.
//
//   (2) FOUND but never before implemented. The four ops.yml `stamp_mode`
//       blends (FIELD.STAMP.MAX / ADD / SUB / REPLACE) have written semantics
//       in design/ops.yml and name `zref::fieldir::stamp_*` reference
//       functions that DO NOT EXIST anywhere in this tree (checked
//       2026-08-19: no definition, and the file design/ops.yml points at,
//       tests/differential/field_stamp_modes.cpp, does not exist either). The
//       semantics are quoted verbatim below and implemented here for the first
//       time. FIELD.STAMP.AGE's ops.yml line says only "decays toward zero by
//       a per-material age rate", which is not arithmetic; the rate is a
//       CHOSEN parameter (see below).
//
//   (3) CHOSEN, with the rejected alternative recorded. The sheet residency
//       directory, the field-driven brush intake, and the `stamp_results`
//       stream. design/blocks.yml says the SURFACE.SHEET residency policy is
//       "per 11"; spec/terrain_rules.md 11 is the list of things EXPLICITLY
//       NOT DECIDED. So there is no law to find, and every rule in
//       `SheetStore` below is a choice, argued in
//       design/contracts/SURFACE.SHEET.md.
//
// Law, in citation order:
//   spec/commands.zidl SurfaceStamp 0x0210 — the frozen wire field set
//     {brush, patch, operation u8, tag u8, strength u16, transform2fx,
//      radius fx16, ring_width fx16}. ABI v3, field set fixed.
//   reference/src/zrender/terrain.cpp `stamp_surface` — the executed law.
//   spec/terrain_rules.md 2 (layer F: 64x64 {tag u8, strength u8}, 8,192 B),
//     7 ("F written only by SURFACE.STAMP"), 8 (the sheets pool),
//     11 (residency is NOT decided).
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 12 — Scar Scribe: 64x64 texels,
//     8-bit tag, 8-bit strength/age, per patch, persistent, and "stamps are
//     deterministic commands and therefore part of captures and replay".
//   design/ops.yml FIELD.STAMP.{MAX,ADD,SUB,REPLACE,AGE} — the five blends.
//   spec/form/field-ir.md 7.1 — the stamp profile I/O record
//     (in u,v:unit, age:u32, strength:unit, p0..p3 -> out tag_op:u32,
//      strength:unit, emissive:unit).
//   spec/qformats.md 2 (unit8 = U 0.0.8, saturate 255).
//
// NOT HERE, deliberately: no height16 scar (layer B is TERRAIN.BAKE's, phase
// 7 — see the seam note in design/contracts/SURFACE.STAMP.md), no breach law,
// no VRAM page layout, no sheet sampling at draw time
// (`zref::render::sample_sheet` owns that and is unchanged), no spline or
// textured-brush primitive (charter 12 lists them; the ABI opcode encodes a
// circle/annulus only, and inventing a wire format for the others would be
// inventing ABI).

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace zref {
namespace surface {

// ---------------------------------------------------------------- geometry --

inline constexpr int kSheetDim = 64;                        // charter 12
inline constexpr int kSheetTexels = kSheetDim * kSheetDim;  // 4,096
inline constexpr int kSheetBytes = kSheetTexels * 2;        // 8,192

/**
 * The patch envelope the sheet is stretched across, fx16 raw — exactly
 * `TerrainPatch::env_x0/z0/x1/z1`, which is what `stamp_surface` reads.
 */
struct Envelope {
  int32_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;
};

/**
 * The geometry half of a SurfaceStamp command, in the ABI's own units.
 * `tx`/`ty` are the transform2fx TRANSLATION; the 2x2 rotation is deliberately
 * absent because a circle/annulus is its own rotation (stamp_surface says so
 * in a comment and never reads r00..r11).
 */
struct StampGeom {
  int32_t tx = 0, ty = 0;  // fx16 raw, world
  int32_t radius = 0;      // fx16 raw, world metres
  int32_t ring_width = 0;  // fx16 raw; <= 0 = filled disc
};

/**
 * THE STATED INPUT DOMAIN, and why it is not the whole word.
 *
 * `stamp_surface` computes `d2 = dx*dx + dz*dz` in int64. `dx` is a
 * difference of two fx16 world coordinates, so |dx| can reach ~2^33 for
 * arbitrary int32 inputs and `dx*dx` then OVERFLOWS int64 — signed overflow,
 * i.e. undefined behaviour in the reference itself. Comparing hardware against
 * a reference that has already left its own arithmetic is testing whose
 * overflow is whose, not whether the block is right (the identical argument
 * design/contracts/TERRAIN.NORMALS.md makes for its own domain).
 *
 * The domain is therefore +-4,096 world metres on every envelope corner, on
 * the transform translation, and on radius/ring_width: fx16 raw magnitude
 * <= 4096 << 16 = 2^28. Then |dx| < 2^30, d2 < 2^61, r*r < 2^57, and every
 * intermediate is exact in 64 bits. 4,096 m is 64 canonical 64-m patches from
 * the island datum in each direction — far outside any island
 * (spec/terrain_rules.md 1.5), so the bound costs nothing real.
 */
inline constexpr int32_t kDomainRaw = 4096 << 16;  // 2^28

inline bool in_domain(int32_t v) { return v >= -kDomainRaw && v <= kDomainRaw; }
inline bool in_domain(const Envelope& e, const StampGeom& g) {
  return in_domain(e.x0) && in_domain(e.z0) && in_domain(e.x1) && in_domain(e.z1) &&
         in_domain(g.tx) && in_domain(g.ty) && in_domain(g.radius) && in_domain(g.ring_width);
}

/**
 * Texel centre in world units, quoted from `stamp_surface`:
 *
 *     wx = ex0 + ((ex1 - ex0) * (2*i + 1)) / 128
 *
 * The division is C++ integer division, i.e. TRUNCATION TOWARD ZERO, and that
 * is not an arithmetic shift when `ex1 - ex0` is negative (an inverted
 * envelope). The RTL reproduces the truncation, not the shift; a shift would
 * disagree by one raw fx16 LSB on inverted envelopes only, which is exactly
 * the kind of divergence that hides until an authoring tool emits one.
 */
inline int64_t texel_wx(const Envelope& e, int i) {
  const int64_t span = static_cast<int64_t>(e.x1) - e.x0;
  return e.x0 + (span * (2 * i + 1)) / 128;
}
inline int64_t texel_wz(const Envelope& e, int j) {
  const int64_t span = static_cast<int64_t>(e.z1) - e.z0;
  return e.z0 + (span * (2 * j + 1)) / 128;
}

/**
 * The coverage test, quoted from `stamp_surface`:
 *
 *     r_outer2 = r*r
 *     r_inner  = rw > 0 ? max(r - rw, 0) : 0
 *     r_inner2 = r_inner*r_inner
 *     covered  = !(d2 > r_outer2 || d2 < r_inner2)
 *
 * Two behaviours worth naming because they look like bugs and are not:
 *  - `r` is used SIGNED and then squared, so a negative radius covers exactly
 *    as a positive one of the same magnitude. Reproduced, not corrected: the
 *    reference is the law and a capture with a negative radius must replay.
 *  - the outer test is `>` and the inner test is `<`, so both radii are
 *    INCLUSIVE. A texel exactly on either rim is stamped.
 */
inline bool covers(const Envelope& e, const StampGeom& g, int i, int j) {
  const int64_t r = g.radius;
  const int64_t rw = g.ring_width > 0 ? g.ring_width : 0;
  const int64_t r_outer2 = r * r;
  const int64_t r_inner = rw > 0 ? (r - rw > 0 ? r - rw : 0) : 0;
  const int64_t r_inner2 = r_inner * r_inner;
  const int64_t dx = texel_wx(e, i) - g.tx;
  const int64_t dz = texel_wz(e, j) - g.ty;
  const int64_t d2 = dx * dx + dz * dz;
  return !(d2 > r_outer2 || d2 < r_inner2);
}

// ------------------------------------------------------------------ blends --

/**
 * The blend vocabulary. Codes 0/1 are the ABI `operation` byte's two executed
 * meanings; codes 2..5 are the ops.yml `stamp_mode` class.
 */
enum Blend : uint8_t {
  kBlendStamp = 0,     // ABI operation 0 (and every value != 1): dst = max(dst, src)
  kBlendDecayAcc = 1,  // ABI operation 1: dst = sat8((dst >> 1) + src)
  kBlendMax = 2,       // ops.yml FIELD.STAMP.MAX     — dst = max(dst, src)
  kBlendAdd = 3,       // ops.yml FIELD.STAMP.ADD     — dst = sat(dst + src)
  kBlendSub = 4,       // ops.yml FIELD.STAMP.SUB     — dst = sat(dst - src)
  kBlendReplace = 5,   // ops.yml FIELD.STAMP.REPLACE — dst = src
  kBlendAge = 6,       // ops.yml FIELD.STAMP.AGE     — dst >>= age_shift
};

/**
 * THE ABI MAPPING, quoted from `stamp_surface`:
 *
 *     if (st.operation == 1) { decay-accumulate } else { keep the peak }
 *
 * Note the `else`: EVERY operation byte other than 1 is the max blend. That is
 * not a defensive default added here, it is the shape of the reference's
 * branch, and a capture carrying operation = 7 replays as a max stamp.
 */
inline Blend blend_of_abi_operation(uint8_t operation) {
  return operation == 1 ? kBlendDecayAcc : kBlendStamp;
}

/**
 * Apply one blend to one texel's strength byte.
 *
 * `age_shift` is CHOSEN and only reaches kBlendAge. ops.yml says AGE "decays
 * toward zero by a per-material age rate" and stops there; a rate needs a
 * number and a rounding. Chosen: a right shift, 0..7, i.e. the rate is a
 * negative power of two and decay is exact and monotone with no rounding law
 * to argue about. REJECTED ALTERNATIVE: a unit8 multiply
 * `(dst * rate + 128) >> 8` (qformats 2's `unit_mul`), which is the more
 * expressive knob and the one to adopt when a material table exists — it costs
 * a multiplier and, worse, a rate of 255/256 never reaches zero, so an aged
 * scar would linger at strength 1 for hundreds of frames. A shift always
 * terminates. Recorded here and in design/contracts/SURFACE.STAMP.md so the
 * choice is visible when the per-material table lands.
 */
inline uint8_t blend_apply(Blend b, uint8_t dst, uint8_t src, uint8_t age_shift) {
  int32_t v;
  switch (b) {
    case kBlendDecayAcc:
      v = (dst >> 1) + src;
      return static_cast<uint8_t>(v > 255 ? 255 : v);
    case kBlendAdd:
      v = static_cast<int32_t>(dst) + src;
      return static_cast<uint8_t>(v > 255 ? 255 : v);
    case kBlendSub:
      v = static_cast<int32_t>(dst) - src;
      return static_cast<uint8_t>(v < 0 ? 0 : v);
    case kBlendReplace:
      return src;
    case kBlendAge:
      return static_cast<uint8_t>(dst >> (age_shift & 7));
    case kBlendStamp:
    case kBlendMax:
    default:
      return src > dst ? src : dst;
  }
}

// --------------------------------------------------------------- the sheet --

/**
 * One resident sheet: layer F, 64x64 {tag u8, strength u8}. Byte-identical in
 * content to `zref::render::SurfaceSheet`; kept as its own type only so the
 * store below can own it. `to_render()`/`from_render()` are the bridge the
 * cross-check uses.
 */
struct Sheet {
  uint8_t tag[kSheetTexels] = {};
  uint8_t strength[kSheetTexels] = {};
  void clear() {
    std::memset(tag, 0, sizeof(tag));
    std::memset(strength, 0, sizeof(strength));
  }
};

/** Residency verdicts (CHOSEN; terrain_rules 11 decides nothing here). */
enum class Residency : uint8_t {
  kHit = 0,        // handle was already resident — contents PERSIST
  kAllocated = 1,  // a free slot was taken and CLEARED to zero
  kOverflow = 2,   // no free slot: the request is REJECTED, nothing written
};

struct AcquireResult {
  Residency status = Residency::kOverflow;
  int slot = -1;
};

/**
 * The resident sheet directory — CHOSEN, every rule of it.
 *
 * design/blocks.yml gives exactly two sentences of policy: "Store 64x64
 * per-patch tag+strength sheets with residency tracking" and "Residency policy
 * per 11; overflow rejects the stamp, never partial-writes." Its 11 is
 * spec/terrain_rules.md 11, the *not decided* list. The second sentence is a
 * real constraint and is obeyed literally. The rest is chosen:
 *
 *  1. FULLY ASSOCIATIVE, keyed by the 32-bit patch handle, `kSheets` slots
 *     (2 by default). REJECTED ALTERNATIVE: direct-mapped on the handle's low
 *     bits, which is one comparator instead of `kSheets` — and which makes two
 *     simultaneously-stamped patches whose handles collide evict each other
 *     every frame, silently destroying persistence. With a resident set of 2
 *     the associativity costs 2 comparators.
 *  2. NEVER EVICT. A full directory rejects the acquire (`kOverflow`); the
 *     stamp then writes NOTHING — not one texel. That is the ledger's
 *     "overflow rejects the stamp, never partial-writes" verbatim, and it is
 *     also the only policy that keeps a replay deterministic without a
 *     recency order in the capture. REJECTED ALTERNATIVE: LRU eviction, which
 *     needs recency state in the capture to replay and which can silently
 *     discard a scar the player can see.
 *  3. RE-ACQUIRING A RESIDENT HANDLE DOES NOT CLEAR. This is persistence, the
 *     entire point of the sheet (charter 12 "persistent"). A fresh slot IS
 *     cleared, because the reference's `SurfaceSheet` is value-initialised and
 *     the software console's `sheet_for()` "creates on first use".
 *  4. `release()` frees a slot; the next acquire of that handle allocates and
 *     clears. There is no partial release and no dirty writeback here — the
 *     VRAM page is MEM.GUARD's and not modelled.
 */
class SheetStore {
 public:
  explicit SheetStore(int slots = 2) : slots_(slots) {
    sheets_.resize(static_cast<size_t>(slots));
    handle_.assign(static_cast<size_t>(slots), 0);
    live_.assign(static_cast<size_t>(slots), 0);
  }

  int slots() const { return slots_; }

  AcquireResult acquire(uint32_t handle) {
    for (int s = 0; s < slots_; ++s)
      if (live_[static_cast<size_t>(s)] && handle_[static_cast<size_t>(s)] == handle)
        return {Residency::kHit, s};
    for (int s = 0; s < slots_; ++s)
      if (!live_[static_cast<size_t>(s)]) {
        live_[static_cast<size_t>(s)] = 1;
        handle_[static_cast<size_t>(s)] = handle;
        sheets_[static_cast<size_t>(s)].clear();
        return {Residency::kAllocated, s};
      }
    return {Residency::kOverflow, -1};
  }

  bool release(uint32_t handle) {
    for (int s = 0; s < slots_; ++s)
      if (live_[static_cast<size_t>(s)] && handle_[static_cast<size_t>(s)] == handle) {
        live_[static_cast<size_t>(s)] = 0;
        return true;
      }
    return false;
  }

  int find(uint32_t handle) const {
    for (int s = 0; s < slots_; ++s)
      if (live_[static_cast<size_t>(s)] && handle_[static_cast<size_t>(s)] == handle) return s;
    return -1;
  }

  bool resident(int slot) const { return live_[static_cast<size_t>(slot)] != 0; }
  Sheet& at(int slot) { return sheets_[static_cast<size_t>(slot)]; }
  const Sheet& at(int slot) const { return sheets_[static_cast<size_t>(slot)]; }

  /** Occupancy mask, LSB = slot 0 — what `residency_status` reports. */
  uint32_t occupancy() const {
    uint32_t m = 0;
    for (int s = 0; s < slots_; ++s)
      if (live_[static_cast<size_t>(s)]) m |= 1u << s;
    return m;
  }

 private:
  int slots_;
  std::vector<Sheet> sheets_;
  std::vector<uint32_t> handle_;
  std::vector<uint8_t> live_;
};

// ------------------------------------------------------------- the stamper --

/**
 * The per-stamp source values. `strength` is the ABI's u16; the conversion to
 * the sheet's byte is `strength >> 8`, quoted from `stamp_surface` — a TRUNCATION,
 * not a round-half-up, so 0xFFFF -> 255 and 0x01FF -> 1. qformats 2 would
 * round `unit8` conversions half-up; the reference does not, and the reference
 * is what the goldens pin. Recorded as a deliberate divergence from the
 * qformats family in design/contracts/SURFACE.STAMP.md.
 */
struct StampSource {
  uint8_t tag = 0;
  uint16_t strength = 0;
  Blend blend = kBlendStamp;
  uint8_t age_shift = 1;
};

/** One texel the stamp actually wrote — the `stamp_results` record. */
struct StampWrite {
  uint16_t texel = 0;    // j*64 + i, scan order
  uint8_t tag = 0;       // value written to layer F tag
  uint8_t strength = 0;  // value written to layer F strength
  uint8_t before = 0;    // strength before the blend (BAKE needs the delta)
};

/**
 * Apply one stamp to one sheet, appending every written texel to `out`.
 *
 * Scan order is j (row) outer, i (column) inner — z-then-x, the tree's one
 * scan order (terrain_rules 4.3, 6.6). Order is observable through
 * `stamp_results`, so it is law, not an implementation detail.
 *
 * `tag` is written on EVERY covered texel regardless of blend, including
 * kBlendSub and kBlendAge which may leave strength at 0. That is
 * `stamp_surface`'s behaviour (`sheet.tag[idx] = st.tag;` sits outside the
 * if/else) and it is reproduced.
 */
inline void stamp_apply(Sheet& sheet, const Envelope& e, const StampGeom& g, const StampSource& s,
                        std::vector<StampWrite>* out) {
  const uint8_t src = static_cast<uint8_t>(s.strength >> 8);
  for (int j = 0; j < kSheetDim; ++j) {
    for (int i = 0; i < kSheetDim; ++i) {
      if (!covers(e, g, i, j)) continue;
      const int idx = j * kSheetDim + i;
      const uint8_t before = sheet.strength[idx];
      sheet.strength[idx] = blend_apply(s.blend, before, src, s.age_shift);
      sheet.tag[idx] = s.tag;
      if (out) out->push_back({static_cast<uint16_t>(idx), s.tag, sheet.strength[idx], before});
    }
  }
}

/**
 * The field-driven brush — CHOSEN, and the requirement it imposes on
 * FIELD.SEQ.STAMP is stated rather than assumed.
 *
 * spec/form/field-ir.md 7.1 gives the stamp profile's I/O record: in
 * (u,v:unit, age:u32, strength:unit, p0..p3) -> out (tag_op:u32,
 * strength:unit, emissive:unit). Nothing in the tree says how those results
 * reach this block, and no `tag_op` bit layout is written down anywhere.
 * Chosen:
 *
 *  1. ONE RESULT PER VISITED TEXEL, in the same j-outer/i-inner scan order,
 *     for all 4,096 texels — NOT one per covered texel. A result whose texel
 *     is not covered is consumed and DISCARDED. REJECTED ALTERNATIVE:
 *     delivering results only for covered texels, which makes the consumption
 *     rate data-dependent on a coverage test that lives in THIS block, so the
 *     two blocks would have to agree on the geometry bit-for-bit or deadlock.
 *     (This is the same discipline design/contracts/TERRAIN.PATCH.md chose
 *     for its field lanes, chosen law 2, for the same reason.)
 *  2. `tag_op` u32 unpacks as tag = bits [7:0], blend = bits [10:8],
 *     age_shift = bits [14:12]. Little-endian bit order in a lane that has no
 *     stated layout at all; chosen so tag occupies the natural low byte.
 *  3. The field's `strength` lane is a u16 carrying the SAME format the ABI
 *     field does, so this block applies ONE strength conversion (`>> 8`) on
 *     both paths. REJECTED ALTERNATIVE: taking the field strength already
 *     reduced to u8, which puts a second, differently-rounded conversion in
 *     FIELD.SEQ.STAMP and guarantees the two paths drift.
 *  4. `emissive` is NOT consumed. Layer F has two bytes and both are spoken
 *     for (charter 12); there is nowhere lawful to put it, and inventing a
 *     third byte would change the frozen 8,192 B layer-F size
 *     (terrain_rules 2). Recorded, dropped.
 */
struct FieldResult {
  uint32_t tag_op = 0;
  uint16_t strength = 0;
};

inline void stamp_apply_field(Sheet& sheet, const Envelope& e, const StampGeom& g,
                              const std::vector<FieldResult>& field, std::vector<StampWrite>* out) {
  for (int j = 0; j < kSheetDim; ++j) {
    for (int i = 0; i < kSheetDim; ++i) {
      const int idx = j * kSheetDim + i;
      const FieldResult& f = field[static_cast<size_t>(idx)];
      if (!covers(e, g, i, j)) continue;
      const uint8_t tag = static_cast<uint8_t>(f.tag_op & 0xFF);
      const Blend b = static_cast<Blend>((f.tag_op >> 8) & 0x7);
      const uint8_t age_shift = static_cast<uint8_t>((f.tag_op >> 12) & 0x7);
      const uint8_t src = static_cast<uint8_t>(f.strength >> 8);
      const uint8_t before = sheet.strength[idx];
      sheet.strength[idx] = blend_apply(b, before, src, age_shift);
      sheet.tag[idx] = tag;
      if (out) out->push_back({static_cast<uint16_t>(idx), tag, sheet.strength[idx], before});
    }
  }
}

}  // namespace surface
}  // namespace zref
