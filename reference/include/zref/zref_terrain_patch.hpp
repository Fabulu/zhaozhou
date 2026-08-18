// zref_terrain_patch.hpp — the TERRAIN.PATCH oracle.
//
// A THIN VIEW onto two ratified laws, not a second implementation of either:
//
//   1. `spec/terrain_rules.md` §3.4's per-vertex composition chain, as
//      `zref::render::compose_lattice` (reference/src/zrender/terrain.cpp)
//      already performs it. compose_lattice runs the chain over a whole 33x33
//      patch with the field interpreter inline; this header exposes the same
//      chain for ONE vertex given the field height lanes already evaluated,
//      because the hardware seam is exactly there (charter: FIELD.SEQ.EARTH
//      evaluates, TERRAIN.PATCH composes). `tests/terrain/terrain_patch_directed`
//      proves the view is faithful by composing a real 33x33 patch with a real
//      earth program through compose_lattice and through this header and
//      requiring every one of the 1,089 vertices to agree bit-for-bit. Without
//      that cross-check this file would be a second implementation, which
//      charter §29-6 forbids.
//
//   2. `spec/terrain_rules.md` §9.1's frozen live-field intake bound
//      (MAX_PATCH_FIELDS = 16) and its overflow law: append in command order,
//      reject the tail, never evict, count in `programs_rejected`, emit a
//      trace event. There is no prior implementation of that law anywhere in
//      the tree — this header is its first, and the RTL matches it.
//
// Law, in citation order:
//   spec/terrain_rules.md §3.4 (composition + the two clamps), §9.1 (the
//     intake bound, the overflow law, the closed-interval binning rule),
//     §4.2 (the composed cache this block produces), §2 (layer A/B/C)
//   spec/qformats.md §2/§9 (height16 -> fx16 is an EXACT raw << 8),
//     §3 (saturating fx_add; one rounding per result)
//   charter §11.4 (bounded field evaluation; priority lives above the seam)
//   design/contracts/TERRAIN.PATCH.md
//
// What this header deliberately does NOT do: evaluate field programs (that is
// `zfield::interpret`, called by FIELD.SEQ.EARTH — never twice, §4.1), decide
// `age`/`phase` or the `frame_tick >= start_tick` gate (dispatch-side, and
// visible in compose_lattice's own loop), bake scars, or evaluate the breach
// law (`zref::terrain::apply_breach_law` owns that).

#pragma once

#include <cstdint>
#include <vector>

#include "zref/zref_fixp.hpp"

namespace zref {
namespace terrain {

// ---- the frozen intake bound (terrain_rules §9.1) --------------------------

/**
 * MAX_PATCH_FIELDS: live earth programs composed per patch per frame. Frozen
 * 2026-08-16 with the 8-wizard donor worst case as the sizing floor (8 held
 * Erupts + 8 Quakes = 16 exactly). A 4-bit lane index, deliberately.
 */
inline constexpr int kMaxPatchFields = 16;

/**
 * One live-field record as the intake sees it: the footprint rectangle the
 * per-vertex test uses, plus the identity the reject trace event carries
 * (terrain_rules §9.1 law 2).
 *
 * The footprint is a CLOSED interval in fx16 world units, matching both
 * compose_lattice's own test (`cx < x0 || cx > x1 || cz < y0 || cz > y1` ->
 * skip) and §9.1's "closed intervals over the shared 33x33 vertex lattice"
 * binning rule. A footprint-border vertex is INSIDE.
 */
struct FieldRecord {
  int32_t x0 = 0;  // fx16 raw
  int32_t z0 = 0;
  int32_t x1 = 0;
  int32_t z1 = 0;
  uint32_t program_hash = 0;  // rides the trace event
  uint16_t cmd_index = 0;     // command-stream index, rides the trace event
};

/** A §9.1 law-2 trace event: one per rejected record, in command order. */
struct RejectEvent {
  uint16_t patch_id = 0;
  uint32_t program_hash = 0;
  uint16_t cmd_index = 0;
};

/**
 * `covers` — the closed-interval footprint test compose_lattice performs at
 * every lattice vertex before adding a field's height lane. Quoted from
 * reference/src/zrender/terrain.cpp:
 *
 *     if (cx < cmd.footprint.x0 || cx > cmd.footprint.x1 ||
 *         cz < cmd.footprint.y0 || cz > cmd.footprint.y1) continue;
 *
 * Note that an INVERTED rectangle (x1 < x0) covers nothing, which is the
 * reference's behaviour too, not a special case added here.
 */
inline bool covers(const FieldRecord& f, int32_t wx, int32_t wz) {
  return !(wx < f.x0 || wx > f.x1 || wz < f.z0 || wz > f.z1);
}

/**
 * The §9.1 bounded intake. Per patch per frame: records append in COMMAND
 * ORDER; the first `kMaxPatchFields` win, every run, identically; records
 * beyond it are REJECTED — not composed, counted, traced — and nothing already
 * listed is ever evicted. Accept/reject is therefore a pure function of the
 * command stream, which is what makes it capture-replay exact.
 *
 * Priority is NOT here and must not be: charter §11.4 assigns it to software
 * above the seam (the CPU sorts droppable cosmetics last per patch and applies
 * the bake / pre-compose / degrade valves before emission). The contract's
 * older "rejects the lowest-priority cosmetic fields" wording was superseded
 * on 2026-08-16 for exactly that reason — the hardware has no priority notion.
 */
class FieldList {
 public:
  void clear() {
    fields_.clear();
    // `programs_rejected` and the trace are NOT cleared: the counter is a
    // frame-life diagnostic across every patch, mirroring how the RTL counter
    // survives a per-patch list clear and only resets on reset.
  }

  void reset() {
    fields_.clear();
    rejected_ = 0;
    trace_.clear();
  }

  /** Offer one record in command order. Returns true iff it was accepted. */
  bool offer(const FieldRecord& f, uint16_t patch_id) {
    if (static_cast<int>(fields_.size()) >= kMaxPatchFields) {
      ++rejected_;
      trace_.push_back(RejectEvent{patch_id, f.program_hash, f.cmd_index});
      return false;
    }
    fields_.push_back(f);
    return true;
  }

  int size() const { return static_cast<int>(fields_.size()); }
  const FieldRecord& operator[](int i) const { return fields_[static_cast<size_t>(i)]; }
  uint32_t programs_rejected() const { return rejected_; }
  const std::vector<RejectEvent>& trace() const { return trace_; }

 private:
  std::vector<FieldRecord> fields_;
  uint32_t rejected_ = 0;
  std::vector<RejectEvent> trace_;
};

// ---- the per-vertex composition (terrain_rules §3.4) -----------------------

/** The three height16 lattice planes at one vertex, plus its placed position. */
struct ComposeIn {
  int16_t base = 0;    // layer A, authored, height16
  int16_t scar = 0;    // layer B, bake-written, height16
  int16_t bottom = 0;  // layer C, the modelled underside, height16
  bool dual = false;   // false = the legacy single-surface page (kind 4)
  int32_t wx = 0;      // placed world x, fx16 raw (the footprint test's input)
  int32_t wz = 0;      // placed world z, fx16 raw
};

/** What one composed vertex produces. */
struct ComposeOut {
  int32_t compose_top = 0;  // base + scar, clamped at bottom (§3.4 line 1)
  int32_t live_top = 0;     // + the field chain, clamped again (§3.4 line 2)
  int32_t bottom = 0;       // fx(bottom), or live_top for a legacy lattice
  bool dirty = false;       // live_top != fx(base): the ground moved here
};

/**
 * `compose_vertex` — §3.4 verbatim, for one vertex.
 *
 *     compose_top = max( fx(base) + fx(scar),  fx(bottom) )
 *     live_top    = max( compose_top + SUM field height lanes (command order,
 *                        fx_add chain),  fx(bottom) )
 *
 * `field_h[i]` is the height out-lane of accepted list entry `i` evaluated at
 * this vertex (FIELD.SEQ.EARTH's job, §4.1: field programs are evaluated ONLY
 * at lattice vertices, by the one interpreter). A lane whose footprint does not
 * cover the vertex is skipped, not added as zero — identical in value, and
 * identical in SatLedger records too, which is why it is written this way.
 *
 * ORDER, AND WHY THE HARDWARE MAY TRANSPOSE THE LOOPS. compose_lattice iterates
 * apps OUTER and vertices INNER, mutating `lat.top[idx]` in place; this
 * function iterates lanes for ONE vertex. Those agree bit-for-bit because
 * `fx_add` is saturating and therefore order-dependent, but the order that
 * matters is the order of adds AT A GIVEN VERTEX, and both forms apply the
 * lanes to a vertex in command order. Nothing about the transpose changes a
 * single add's operands. The RTL composes vertex-major for the same reason:
 * it needs no lattice-sized accumulator.
 *
 * height16 -> fx16 is the EXACT `raw << 8` of qformats §2/§9 — no rounding
 * exists in the up-conversion, and there is no other rounding in this function
 * at all. `fx_add` saturates and records (§3).
 */
inline ComposeOut compose_vertex(const ComposeIn& in, const FieldList& list, const int32_t* field_h,
                                 SatLedger* L = nullptr) {
  const int32_t base_fx = static_cast<int32_t>(in.base) << 8;
  const int32_t scar_fx = static_cast<int32_t>(in.scar) << 8;
  const int32_t bot_fx = static_cast<int32_t>(in.bottom) << 8;

  int32_t top = fx_add(fx16{base_fx}, fx16{scar_fx}, L).raw;
  if (in.dual && top < bot_fx) top = bot_fx;  // clamp at the underside (§3.4)

  ComposeOut out;
  out.compose_top = top;

  for (int i = 0; i < list.size(); ++i) {
    if (!covers(list[i], in.wx, in.wz)) continue;
    top = fx_add(fx16{top}, fx16{field_h[i]}, L).raw;  // the height lane
  }
  // live_top = max(compose_top + fields, bottom): the ONE clamp after the
  // command-order fx_add chain (§3.4) — a transient wave can never punch below
  // the underside, so it can never fake a breach.
  if (in.dual && top < bot_fx) top = bot_fx;

  out.live_top = top;
  out.bottom = in.dual ? bot_fx : top;
  out.dirty = top != base_fx;
  return out;
}

// ---- the subpatch dirty mask (charter §11.1 + terrain_rules §4.4) ----------

/**
 * The 4x4 subpatch grid of one 32x32-cell patch (charter §11.1: "divided into
 * sixteen 8x8-cell subpatches"), as a 16-bit mask, bit `row * 4 + col`.
 *
 * LAW CHOSEN, NOT FOUND — argued in design/contracts/TERRAIN.PATCH.md. A
 * vertex marks a subpatch dirty when its `live_top` differs from its authored
 * `fx(base)`, i.e. when the ground has actually moved there. Border vertices
 * are physically shared, so a vertex on a subpatch boundary marks BOTH
 * neighbours — the same closed-interval reasoning §9.1 uses for footprint
 * binning, and the reason a corner vertex marks four.
 *
 * The rejected alternative was to mark from the field footprint rectangles
 * alone: cheaper, but a crater's bounding rectangle is dirty in its corners
 * where the field evaluates to exactly zero, so it marks subpatches whose
 * ground did not move and defeats §4.4's "dirty patches only" whole purpose.
 */
inline uint16_t subpatch_mask(int vi, int vj) {
  const int col_lo = vi == 0 ? 0 : (vi - 1) >> 3;
  const int col_hi = (vi >> 3) > 3 ? 3 : (vi >> 3);
  const int row_lo = vj == 0 ? 0 : (vj - 1) >> 3;
  const int row_hi = (vj >> 3) > 3 ? 3 : (vj >> 3);
  uint16_t m = 0;
  for (int r = row_lo; r <= row_hi; ++r)
    for (int c = col_lo; c <= col_hi; ++c) m |= static_cast<uint16_t>(1u << (r * 4 + c));
  return m;
}

}  // namespace terrain
}  // namespace zref
