// zref_creature.hpp — creature/character reference core (world-identity wave,
// the creature/character LOD + deformation lane).
//
// The ORACLE the Phase 8-9 GEOM blocks (VDECODE/POSE/SKIN/MESHFETCH) will be
// verified against — charter 21 order: reference BEFORE RTL. Nothing here is
// RTL and nothing here may promote a block past what its evidence supports.
//
// Law (in citation order):
//   spec/creature_rules.md  1.1/1.2 ring-cylinder parts -> meshlets (<=64
//                             unique verts, <=126 tris, one material per
//                             part), <=32 bones HARD, <=2 influences with
//                             weights in 1/64 quanta
//                           2.1 clip bank: root displacement 3 x fx16 +
//                             8 B/bone/frame quantized quats, keys at 30 Hz
//                             shown 2 sim ticks, HARD-CUT transitions,
//                             keyframe event tags {frame u16, event u8,
//                             param u8} <=4/frame
//                           2.2 pose pipeline: decode-on-fetch into a shared
//                             decoded-pose cache (bake-everything-at-load was
//                             REJECTED — x6 memory; this header implements
//                             the decision, and the cache makes the runtime
//                             pose a table lookup on hit)
//                           3    the 3->2 weight clamp gate (warn 1%,
//                             reject 3% of bound radius)
//                           4.2  rotateOnGround slope tilt, bulk inflation,
//                             tick-skip slow-motion
//   spec/qformats.md        2 fx16 S 1.15.16, unit8, angle16 turns
//                           3 single-rounding law (A3b) — every multiply
//                             chain below computes the EXACT wide-integer
//                             expression then rounds ONCE
//                           4 rescale(): the one rounding primitive
//                           7.5 noise2_hash for deterministic gib velocities
//   charter                 9 The Measure: screen-error LOD, hysteresis,
//                             minimum hold (the ladder here: mesh ->
//                             micro-mesh -> splat -> glint)
//                           10 meshlet unit + two-weight skinning
//                          29-6 one semantics (terrain taps reuse
//                             zref::terrain::column_query; the flat-shade
//                             law is zrender's shade_points, shared)
//                          29-7 no host floats in deterministic paths —
//                             everything below is integer-only
//
// QUATERNION LANE FORMAT (PROPOSED, NOT FROZEN — flags the qformats 13
// change control): S 1.0.14 per lane, i.e. raw = round_half_up(q * 2^14),
// |raw| <= 16384, packed s16[4] = the 8 B/bone/frame of creature_rules 2.1.
// Decode = the 9-product quat->matrix formula, ONE rescale(.,11) per
// element, NO renormalization (GEOM.POSE contract). The measured worst-case
// error is asserted in tests/geometry/creature_core.cpp and must be
// re-stated with any lane change.

#pragma once

#include "zref/zref_fixp.hpp"
#include "zref/zref_sat.hpp"
#include "zref/zref_terrain.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace zref {

// forward: the creature page is a render Tileset (256 tiles of 64x64
// CLUT8 with an RGB565 palette), declared here to avoid an include cycle
namespace render {
struct Tileset;
}

// forward: the direct-colour page variant (zref_texture.hpp) — RGB565 mip
// chains sampled through the one Tmu law
struct DirectPageSet;

namespace creature {

// ---------------------------------------------------------------- formats --

/** Quantized unit quaternion: s16[4] (w, x, y, z), each lane S 1.0.14. */
struct quat16 {
  int16_t q[4];
};

/** Lane scale: 1.0 == 16384 (S 1.0.14). */
inline constexpr int32_t kQuatOne = 16384;

/** The identity rotation (w=1). */
inline constexpr quat16 quat16_identity() { return quat16{{kQuatOne, 0, 0, 0}}; }

/**
 * Quantize a unit quaternion given as fx16 lanes. Hemisphere-canonical
 * (negate all when w < 0, or w == 0 and x < 0, or w == x == 0 and y < 0 ...)
 * so (q and -q) quantize IDENTICALLY — clip compression must not care which
 * hemisphere the author exported. Each lane round-half-up to 2^14, saturate
 * s16. Integer-only.
 */
quat16 quat16_quantize(fx16 w, fx16 x, fx16 y, fx16 z);

/**
 * Axis-angle authoring: unit axis (fx16) + half-angle sin/cos (fx16, from
 * the fx_sin table) -> quat16. cos is the w lane; the vector lanes are
 * axis * sin, each single-rounded (fx_mul). Deterministic authoring path
 * for tests and reel fixtures.
 */
quat16 quat16_axis_angle(fx16 ax, fx16 ay, fx16 az, fx16 half_sin, fx16 half_cos);

/** 3x4 affine matrix, row-major fx16: rows m00 m01 m02 tx / m10.. ty / m20.. tz. */
struct mat3x4fx {
  int32_t m[12];
};

/** The identity affine. */
inline constexpr mat3x4fx mat3x4_identity() {
  return mat3x4fx{{1 << 16, 0, 0, 0, 0, 1 << 16, 0, 0, 0, 0, 1 << 16, 0}};
}

/**
 * quat16 -> rotation matrix (GEOM.POSE decode core). The 9-product formula:
 * with lanes Qw..Qz (S 1.0.14 raw),
 *   diag_ijj = 2^16 - rescale(Qa^2 + Qb^2, 11)
 *   off_ij   = rescale(Qa*Qb +- Qc*Qd, 11)
 * Each element: exact s64 products, ONE round-half-up rescale, saturate.
 * NO renormalization (creature_rules 2.2 decision; the quantization scale
 * error bound is measured in the tests and rides the future qformats
 * amendment). Translation column zero.
 */
void quat16_to_mat3(const quat16& q, mat3x4fx& out, SatLedger* L);

/**
 * Affine product out = a * b. Per element: exact s128 sum of the (up to
 * three) 32x32 products (+ the translation term), then ONE rescale(.,16) +
 * saturate — the qformats 2 mat4fx x vec4 rule applied to 3x4 (A3b).
 */
void mat3x4_mul(const mat3x4fx& a, const mat3x4fx& b, mat3x4fx& out, SatLedger* L);

/**
 * Rigid inverse: transpose the 3x3 (exact), t' = -R^T t (one rounding per
 * component). Correct for rotation-only inputs; used at bake time on rest
 * transforms whose 3x3 is EXACTLY a rotation or identity.
 */
void mat3x4_invert_rigid(const mat3x4fx& in, mat3x4fx& out, SatLedger* L);

// -------------------------------------------------------------- skeleton ---

/** Hard donor law: <=32 bones per creature (creature_rules 1.2). */
inline constexpr int kMaxBones = 32;

/**
 * One bone: parent index (parent-before-child order REQUIRED, validated at
 * bake) and the rest LOCAL translation (fx16). Rest rotations are identity
 * — the bind convention of the ring format: rings are authored in rest
 * orientation, so B_rest is a pure translation chain and its inverse is
 * EXACT (translate(-world_rest_pos), zero rounding).
 */
struct Bone {
  uint8_t parent;
  int32_t tx, ty, tz;  // fx16
};

struct Skeleton {
  uint8_t bone_count = 0;
  std::array<Bone, kMaxBones> bones{};
};

/**
 * Load-time bake of the skeleton (the one place rest inverses are computed):
 * inv_rest[b] = B_rest_b^{-1}, exact because rest rotations are identity.
 * Also caches world_rest[b] (fx16) for attachment-point math.
 */
struct SkeletonBake {
  std::array<mat3x4fx, kMaxBones> inv_rest{};
  std::array<int32_t, kMaxBones> world_x{}, world_y{}, world_z{};  // fx16
};

/** Validate (parent-before-child, no cycles by construction, count <= 32). */
bool bake_skeleton(const Skeleton& sk, SkeletonBake& out);

// -------------------------------------------------------------- clip bank --

/** Keyframe event tag (creature_rules 2.1): {frame u16, event u8, param u8}. */
struct ClipEvent {
  uint16_t frame;
  uint8_t event;
  uint8_t param;
};

/** Event vocabulary (the sim consumes these; hardware never emits them). */
enum EventKind : uint8_t {
  kEvAttack = 1,
  kEvShoot = 2,
  kEvLoad = 3,
  kEvCast = 4,
  kEvGrab = 5,
  kEvFoot = 6,
  kEvSound = 7,
};

/**
 * Optional bind-space radial deformation sample. Both lanes are Q0.16
 * DELTAS from identity: flatten contracts the authored radial axis and spread
 * expands its perpendicular plane. {0,0} is exact identity. The positive-volume
 * gate rejects flatten == 1.0; ordinary clips carry no samples at all.
 */
struct DeformSample {
  uint16_t flatten = 0;
  uint16_t spread = 0;
};

/**
 * One clip: slot id, frame_count keys, per-frame root displacement (3 x fx16)
 * and bone_count quantized quats (8 B/bone/frame as authored), plus the
 * event tags. Frames loop (the donor's walk/idle cycles); events replay with
 * the loop. <=4 events per frame is VALIDATED at compile (compile_creature).
 */
struct Clip {
  uint16_t slot_id = 0;
  uint16_t frame_count = 0;
  std::vector<int32_t> root;      // 3 * frame_count fx16 (x, y, z per frame)
  std::vector<quat16> quats;      // frame_count * bone_count
  std::vector<ClipEvent> events;  // frame-sorted
  // Optional narrow deformation sidecar. Empty means exact identity; otherwise
  // one fixed-point sample per authored key. It never enters the PoseBank, so
  // decoded bone matrices remain rigid and shareable.
  std::vector<DeformSample> deform;
  /**
   * PRESENTATION INTERPOLATION (added 2026-08-26, MODELINGGUIDE section 8).
   *
   * Keys are authored at 30 Hz and held for two 60 Hz sim ticks, so motion
   * updates 30 times a second no matter how fast the clip actually moves. On
   * a slow idle that is invisible; on a triple somersault and on a falling
   * flail it is plainly choppy.
   *
   * With this set, the pose decoder blends between key f and key f+1 at the
   * half-tick using a NORMALIZED integer quaternion lerp. It is PRESENTATION
   * ONLY -- the sim clock, the event frames and every gameplay consequence
   * still run on the authored 30 Hz keys, so determinism is untouched.
   *
   * Default OFF, so nothing that already exists changes by a single bit.
   */
  bool interpolate = false;
  /**
   * HOLD-LAST (2026-08-28, run 0326). Frames loop by default (the donor's
   * cycles), and the presentation interpolation wraps the final key toward
   * key 0 to keep the loop seam smooth. A ONE-SHOT clip (death) holds its
   * last key instead -- and there the wrap is a defect: the held corpse
   * blended half-way back to the stance and the last shown frame flashed
   * the animal alive (caught on the run-0326 death contact sheet, final
   * thumbnail). With this set, interpolation CLAMPS at the last key: the
   * root and quat midpoints of the final segment are the final key itself.
   * Presentation only; authored keys, events and the sim clock untouched.
   */
  bool hold_last = false;
  /**
   * A1 — THE BAKED 60 Hz PRESENTATION COMPANION (2026-08-28, kind-9
   * unfrozen). When present (bake60 on the bank, baked by
   * compile_creature), the half-tick pose is an EXPLICIT baked midpoint
   * instead of the runtime nlerp: root midpoints are Catmull-Rom cubics
   * CLAMPED to the segment interval (monotone near impact/burial by
   * construction — a baked curve cannot overshoot its keys), and quat
   * midpoints are the 4-tap Catmull-Rom of hemisphere-aligned lanes,
   * renormalized, falling back to the plain nlerp midpoint on segments
   * adjacent to an event frame (the event's timing is gameplay truth and
   * its pose must not be smoothed across). PRESENTATION ONLY: keys,
   * events and the sim clock are untouched; sub=1 simply selects a
   * better pose than nlerp could make at runtime. Hero-tier opt-in —
   * ordinary creatures keep the nlerp fallback (the pose-cache economy
   * is sized for armies, not for every creature carrying double frames).
   */
  std::vector<quat16> mid_quats;         // frame_count * bone_count, or empty
  std::vector<int32_t> mid_root;         // frame_count * 3, or empty
  std::vector<DeformSample> mid_deform;  // frame_count, or empty
};

/**
 * Explicit ownership for exceptional authored 60 Hz companion samples.
 *
 * Provenance lives beside a Clip rather than inside the shipped clip. One byte
 * owns channels at one segment's true half-key; every unowned channel is
 * regenerated from the current integer keys during creature compilation.
 */
enum PresentationMidpointChannel : uint8_t {
  kMidpointQuatsAuthored = 1u << 0,
  kMidpointRootAuthored = 1u << 1,
  kMidpointDeformAuthored = 1u << 2,
};

struct PresentationMidpointAuthorship {
  uint16_t slot_id = 0;
  std::vector<uint8_t> channels;  // frame_count masks, indexed by segment key
};

/**
 * PHASE-SEAM DECLARATION (C2, 2026-08-28). The hard-cut law means clip
 * transitions blend nothing: a programmable choreography built from phase
 * clips (compress -> coil -> unroll -> spear -> ...) stays pop-free only if
 * the poses on both sides of every intended cut are BIT-IDENTICAL. That is
 * an asset invariant, so the asset compiler enforces it: each declared pair
 * (slot_a, key_a) == (slot_b, key_b) is byte-compared (quats + root) at
 * compile_creature time and the compile FAILS on any mismatch -- the seam
 * law is checked where the bytes are made, not trusted to the eye.
 */
struct SeamPair {
  uint16_t slot_a, key_a;
  uint16_t slot_b, key_b;
};

/** The 64-slot clip bank (creature_rules 2.1; slot ids need not be dense). */
struct ClipBank {
  uint8_t bone_count = 0;
  std::vector<Clip> clips;
  std::vector<SeamPair> seams;  // enforced by compile_creature (C2)
  bool bake60 = false;          // A1: bake presentation midpoints at compile
};

/** Byte size of one frame as shipped: 12 + 8 * bone_count (creature_rules 2.1). */
inline constexpr uint32_t clip_frame_bytes(uint8_t bone_count) { return 12u + 8u * bone_count; }

// ----------------------------------------------------- pose bank (GEOM.POSE) —

struct CreatureType;  // defined below (compile product)

/**
 * The decoded-pose cache (creature_rules 2.2 DECISION: decode-on-fetch).
 * (type, clip, frame) tuples decode on miss into <=32 skinning matrices
 * and stay resident; instances of one type playing one clip at one tick
 * SHARE the palette (the type-grouped army economy). On hit the runtime
 * pose is a table lookup — no per-frame math, which is the property the
 * hardware needs and the reason the cache exists at all.
 *
 * Cache law (GEOM.POSE contract): 128 tuples; LRU eviction NEVER touches a
 * tuple referenced this frame (begin_frame clears the marks); a frame whose
 * distinct-tuple demand exceeds capacity is a content-tier violation —
 * counted (clamped_inserts) and clamped deterministically (decode without
 * insert). Bad clip/frame ids: identity bind pose + bad_ids counter, never
 * a wild read.
 *
 * Determinism: the decoded CONTENT of a tuple never depends on request
 * order; palettes are pure functions of (type, clip, frame). Asserted by
 * the tests over shuffled request orders.
 */
class PoseBank {
 public:
  static constexpr size_t kCacheTuples = 128;

  struct Counters {
    uint32_t hits = 0;
    uint32_t misses = 0;
    uint32_t bad_ids = 0;
    uint32_t clamped_inserts = 0;
  };

  /** Clear the referenced-this-frame marks (call at frame start). */
  void begin_frame();

  /**
   * Palette for (type, slot, frame): pointer to 32 mat3x4fx (bones past
   * bone_count are identity). Never nullptr. On a bad slot/frame id the
   * pointer targets the static identity bind pose and bad_ids increments.
   */
  const mat3x4fx* acquire(const CreatureType& type, uint16_t slot, uint16_t frame, uint8_t sub = 0);

  const Counters& counters() const { return ctr_; }
  size_t resident() const { return resident_; }

 private:
  struct Slot {
    bool valid = false;
    uint16_t type = 0, clip = 0, frame = 0;
    uint8_t sub = 0;  // half-key phase, for presentation interpolation
    uint64_t lru = 0;
    bool this_frame = false;
    std::array<mat3x4fx, kMaxBones> pose{};
  };
  std::array<Slot, kCacheTuples> slots_{};
  size_t resident_ = 0;
  uint64_t lru_ctr_ = 0;
  Counters ctr_{};
  std::array<mat3x4fx, kMaxBones> scratch_{};  // clamped-insert decode target
};

// ------------------------------------------------------------- skinning ----

/**
 * The compiled skin vertex (meshlet payload). Position fx16 in bind space;
 * <=2 influences with w0 in 1/64 quanta (w1 = 64 - w0; rigid = w0 64 /
 * b1 == b0); u/v 8-bit texcoords (the ring builder's U-from-angular-
 * alignment and V-along-rings lanes). NO colour lane — see the colour-lane
 * finding recorded in the run FINDINGS: lighting is computed at render from
 * the flat normal + part material, exactly like terrain.
 */
struct SkinVertex {
  int32_t x, y, z;  // fx16
  uint8_t b0 = 0, b1 = 0;
  uint8_t w0 = 64;  // 1/64 quanta; 64 == rigid
  uint8_t u = 0, v = 0;
  // Packed bind-space smooth normal, S1.7 (127 == 1.0) — the lane charter
  // §10 reserved ("compact normal and UV encoding"; kind-8 layouts unfrozen
  // until Phase 12). (0,0,0) means NO normal: consumers fall back to flat
  // face shading, so every pre-normal asset renders exactly as before.
  // GENERATED by compile_creature (area-weighted, position-keyed so seam
  // and meshlet-boundary duplicates carry identical normals; the micro
  // rung's are recomputed from micro topology). s8x3 over octahedral by
  // the V2 brief's own argument: no decode hardware, simple fixed dots.
  int8_t nx = 0, ny = 0, nz = 0;
};

/** Optional parallel metadata for one compiled vertex. */
enum class DeformRole : uint8_t {
  kNone = 0,
  // Scale the real vertex around centre: contract `axis`, expand the two
  // perpendicular lanes. Normals receive the fixed-point inverse transpose.
  kRadial = 1,
  // Translate an attachment by the carrier point's contraction only. Its own
  // dimensions and normals remain rigid, so markings/fins follow but are not
  // crushed or spread away from their carrier.
  kFollower = 2,
};

struct DeformVertex {
  int32_t center_x = 0, center_y = 0, center_z = 0;     // fx16 bind-space centre
  int32_t carrier_x = 0, carrier_y = 0, carrier_z = 0;  // follower sample point
  DeformRole role = DeformRole::kNone;
  uint8_t axis = 0;      // bind-space cardinal axis: 0=x, 1=y, 2=z
  uint8_t strength = 0;  // 0..255 local authority
};

/** Resolve the optional authored/presentation sample; bad/absent means identity. */
DeformSample deformation_sample(const CreatureType& type, uint16_t slot, uint16_t frame,
                                uint8_t sub = 0);

/**
 * Apply one bind-space sidecar sample, returning a regular SkinVertex for the
 * unchanged rigid skin path. Identity exits before arithmetic. kRadial also
 * applies inverse-transpose normal correction and integer renormalisation.
 */
SkinVertex deform_skin_vertex(const SkinVertex& v, const DeformVertex& meta,
                              const DeformSample& sample);

/**
 * Skin one vertex against a decoded palette. Single-rounding law: the FULL
 * expression w0*(Sa v) + w1*(Sb v) is evaluated exactly in s128 (products
 * are 64x64 -> 128) and rounded ONCE with rescale(., 22) (16 fraction bits
 * of the matrix product + 6 of the 1/64 weight scale) — no double rounding
 * between skin and blend (qformats 3, A3b). Rigid vertices take the
 * rescale(.,16) path.
 */
void skin_vertex(const mat3x4fx* palette, const SkinVertex& v, int32_t& ox, int32_t& oy,
                 int32_t& oz, SatLedger* L);

/**
 * Transform a packed bind-space normal through the same two-bone linear blend
 * as its vertex, NORMALISE the blended direction, then take one clamped
 * Lambert against a unit Q16.16 vector FROM the surface TOWARD the light
 * source (not the incoming ray-travel direction). Blending the already-clamped
 * response
 * of each bone is not equivalent: it makes light follow influence weights
 * instead of the deformed surface whenever the two bones disagree.
 */
int32_t skin_normal_lambert(const mat3x4fx* palette, const SkinVertex& v, int32_t lx, int32_t ly,
                            int32_t lz);

// -------------------------------------------------------------- meshlets ---

inline constexpr int kMeshletMaxVerts = 64;  // charter 10
inline constexpr int kMeshletMaxTris = 126;  // charter 10 (96..126 band)

/** One compiled meshlet (charter 10): <=64 verts, <=126 tris, one material. */
struct Meshlet {
  std::vector<SkinVertex> verts;
  // Optional parallel sidecar. Empty is exact identity; otherwise size exactly
  // matches verts. Full and compiler-decimated micro rungs are built from the
  // same RingSpec metadata rather than copied between topologies.
  std::vector<DeformVertex> deform;
  std::vector<uint8_t> idx;           // 3 * tri_count vertex indices
  uint8_t r = 128, g = 128, b = 128;  // part material (the CLUT8 page stand-in)
  uint8_t page = 255;                 // tile in the creature's Tileset; 255 = flat colour
};

// ----------------------------------------------------------- ring builder --

/** One ring of a generalized cylinder: centre height + radius (fx16). */
struct RingSpec {
  int32_t y;       // fx16 along the part axis
  int32_t radius;  // fx16
  uint8_t segments;
  // CHAIN MODE ONLY (see RingPart::chain). This ring s two bones and the
  // weight of the first in 1/64 quanta; w1 = 64 - w0 by construction, so the
  // normalisation law cannot be violated. Ignored by rigid parts.
  uint8_t b0 = 0, b1 = 0, w0 = 64;
  // OFFSET rings (added 2026-08-26). The ring centre, perpendicular to the
  // part axis. A stack of rings is otherwise a straight tube, so a curved form
  // -- an S-posture serpent, a bent neck -- had to be chopped into separately
  // oriented rigid parts, which is what opened the cracks. With an offset the
  // curve lives in the ring table and the surface stays continuous.
  int32_t cx = 0, cz = 0;
  // ELLIPTICAL rings (added 2026-08-26). Per-axis radii; both zero means
  // circular, using `radius`. A rotationally symmetric stack cannot express a
  // flattened tail blade or a skull that is wider than it is deep, and
  // MODELINGGUIDE asks for both.
  int32_t rx = 0, rz = 0;
  // Optional radial-deformation authorship, expressed in this part's local
  // bind axes and quarter-turned with the ring. `deform_center_*` is the tube
  // centre about which the sample acts. A follower uses the ring centre as its
  // carrier point. Defaults are exact identity and emit no compiled sidecar.
  DeformRole deform_role = DeformRole::kNone;
  uint8_t deform_axis = 0;      // local cardinal axis, 0=x, 1=y, 2=z
  uint8_t deform_strength = 0;  // 0..255
  int32_t deform_center_x = 0, deform_center_y = 0, deform_center_z = 0;
};

inline constexpr uint8_t kCapTop = 1;  // fan cap closing the +Y end
inline constexpr uint8_t kCapBot = 2;  // fan cap closing the -Y end

/**
 * A body part in the AUTHORING (tool-side) ring format (creature_rules 1.1).
 * Rings stack along local +Y; each ring has `segments` vertices around it.
 * `align` is the 8-bit angular alignment (the donor's per-entry rotation):
 * vertex k of ring 0 sits at angle (k * 256 / segments + align)/256 turns,
 * and U = that angle's high byte — the -> U texcoord law.
 *
 * `pitch_q`/`yaw_q` orient the part in bind space by EXACT quarter turns
 * (matrices with entries in {0, +-65536} — zero rounding, the all-integer
 * construction preserved): a lying body cylinder is rings stacked along +Y
 * with pitch_q = 1 (+Y -> +Z), legs stay upright, heads tilt by quarters.
 */
struct RingPart {
  std::vector<RingSpec> rings;
  uint8_t caps = 0;
  uint8_t align = 0;
  // NOT a donor law, despite what this comment said until 2026-08-26: the
  // donor is 34.92% multi-bone (measured, FINDINGS-R1 section E.3). The
  // mis-attribution is what authored the cracks.
  uint8_t bone = 0;  // RIGID parts only: the single bone this part follows
  /**
   * CHAIN MODE (added 2026-08-26). A rigid part is one bone, so a shape that
   * bends must be several parts -- and adjacent parts share no vertices, which
   * is why the first Zixxtrixx opened a 61 mm hole (5.3 px on a 19 px body) at
   * the peak of its attack. Texture cannot cover a geometric hole.
   *
   * With `chain` set, each ring carries its OWN {b0, b1, w0} and the rings are
   * authored directly in creature-global bind space (no per-part bone offset).
   * One part can then span a whole bone chain as ONE continuous surface:
   * interior rings are shared by construction, there are no internal caps, and
   * rings near a joint blend across the two bones either side of it.
   *
   * Nothing downstream needed changing. `skin_vertex` already takes the
   * 2-weight path, GEOM.SKIN already specifies the identical
   * rescale(w0*pa + w1*pb, 22), and the measured DSP cost of the weight
   * multiplies was approximately zero. The blend was implemented everywhere
   * EXCEPT in the one place that emits vertices.
   *
   * The reusable primitive this gives us is a continuous flexible chain --
   * snakes, tails, tentacles, long necks.
   */
  bool chain = false;
  /**
   * FEATURE-PRESERVING MICRO CONTROLS. The default compiler rung still keeps
   * every second ring and halves radial segments. Painted or silhouette-critical
   * parts can independently retain either axis, so a coarse LOD cannot turn a
   * bounded marking into disconnected fan triangles. These are authoring knobs,
   * not a screen-space inference; the resulting rung error is still measured.
   */
  bool micro_keep_rings = false;
  bool micro_keep_segments = false;
  uint8_t pitch_q = 0;  // quarter turns about X applied at build
  uint8_t yaw_q = 0;    // quarter turns about Y applied at build
  uint8_t r = 128, g = 128, b = 128;
  // TEXTURE PAGE (added 2026-08-26). Index of the tile in the creature's
  // Tileset. 255 = untextured, and the flat r/g/b above is used instead --
  // which is what every part did before a page pipeline existed.
  uint8_t page = 255;
  // V RANGE on the page (T4, 2026-08-28): the part's rings span page V rows
  // v0..v1 instead of always 0..255, so several parts can share ONE
  // continuous atlas (U = circumference, V = nose-to-tail) with the seam
  // ring's V agreeing on both sides. Defaults keep every existing part
  // bit-identical.
  uint8_t v0 = 0;
  uint8_t v1 = 255;
};

/**
 * Expand rings into meshlets (the asset compiler's job, done here in
 * reference form): consecutive rings are zig-zag stitched — equal segment
 * counts alternate the quad diagonal (the zig-zag); unequal counts take the
 * integer zipper walk (advance the side whose fractional position lags),
 * which is the donor's ring-merge. Caps are fans from the ring centre.
 * Parts larger than a meshlet split at ring boundaries (creature_rules 1.2).
 * All-integer by construction.
 */
std::vector<Meshlet> build_ring_part(const RingPart& part);

/**
 * Triangle-count law (the hand-computable anchor): equal-segment rings give
 * (ring_count - 1) * 2 * segments side triangles plus `segments` per closed
 * cap.
 */
inline constexpr int ring_side_tris(int ring_count, int segments) {
  return (ring_count - 1) * 2 * segments;
}

// ------------------------------------------------------------- creature ----

/**
 * The compiled creature type: skeleton + bake, the mesh/micro meshlet lists,
 * and the LOD ladder's compiler-generated geometric errors (charter 9: LOD
 * is compiler-generated screen-space-error collapse, never artist faces —
 * creature_rules 7). Errors are world-space fx16:
 *   mesh  = 0 (reference)
 *   micro = measured max vertex deviation of the decimated rings (computed)
 *   splat = bound_radius / 2 (a splat cluster replaces the form by its
 *           bound disc: worst silhouette error is the radius itself; half
 *           is the accepted authored constant, recorded here)
 *   glint = bound_radius (a point has no interior)
 */
struct CreatureType {
  uint16_t type_id = 0;  // pose-cache tuple key lane (index into the type table)
  Skeleton skeleton;
  SkeletonBake baked;
  ClipBank bank;
  std::vector<Meshlet> mesh;   // rung 0
  std::vector<Meshlet> micro;  // rung 1 (compiler-decimated)
  int32_t bound_radius = 0;    // fx16, max |bind vertex| (isqrt, exact floor)
  int32_t micro_error = 0;     // fx16, measured
  int32_t splat_error = 0;     // fx16, bound_radius / 2
  int32_t glint_error = 0;     // fx16, == bound_radius
  // The creature's texture page. Null means every meshlet falls back to
  // its flat material colour, so an untextured creature still renders.
  const render::Tileset* page_set = nullptr;
  // Direct-colour page (RGB565 + bilinear + mips through the one TMU law
  // — zref_texture.hpp DirectPageSet). When set it WINS over page_set:
  // the CLUT8 page remains the ordinary-creature format tier
  // (creature_rules 1.2 as amended: the page carries a FORMAT tag).
  const DirectPageSet* page_direct = nullptr;
};

/**
 * Compile: bake the skeleton, expand + gather all parts' meshlets, measure
 * the bound radius, build the micro rung (every 2nd ring, segments halved,
 * min 3 — the decimation constants recorded here) and MEASURE its geometric
 * error against the full surface, validate clips (bone_count match, <=4
 * events/frame, frame-sorted events). Returns false + reason on a malformed
 * input (fail-safe: nothing compiled).
 */
/** A1: regenerate Clip::mid_*; preserve only explicitly owned channels. */
void bake_presentation_midpoints(Clip& c, uint8_t bone_count);
void bake_presentation_midpoints(Clip& c, uint8_t bone_count,
                                 const std::vector<uint8_t>& authored_channels);

bool compile_creature(const Skeleton& sk, const ClipBank& bank, const std::vector<RingPart>& parts,
                      CreatureType& out, const char** reason);
bool compile_creature(const Skeleton& sk, const ClipBank& bank, const std::vector<RingPart>& parts,
                      CreatureType& out, const char** reason,
                      const std::vector<PresentationMidpointAuthorship>& midpoint_authorship);

/**
 * The decode itself (pure, order-independent): the per-bone chain
 *   R   = quat16_to_mat3(quats[frame][b])
 *   LR  = R with translation (rest tx,ty,tz) + root displacement at b == 0
 *   A_b = (b == 0) ? LR : A_parent * LR
 *   S_b = A_b * inv_rest[b]
 * Two matrix multiplies per bone (miss-only cost — the cache exists so this
 * is not per-frame-per-instance). Also the clamp gate's oracle.
 */
void decode_pose(const CreatureType& type, const Clip& clip, uint16_t frame,
                 std::array<mat3x4fx, kMaxBones>& out, SatLedger* L, uint8_t sub = 0);

/**
 * Normalized integer lerp between two quantized quaternions, at t = num/den.
 * Hemisphere-corrected (q and -q are the same rotation, so the nearer of the
 * two is used), then renormalized -- a plain lerp shortens the quaternion and
 * quat16_to_mat3's 9-product formula would scale the whole matrix by |q|^2,
 * which shows up as the creature breathing when it should not.
 */
quat16 quat16_nlerp(const quat16& a, const quat16& b, int32_t num, int32_t den);

// ------------------------------------------------- the 3->2 weight clamp ----

/** Source vertex carrying 3 influences (the authoring rig's export). */
struct SourceVertex {
  int32_t x, y, z;  // fx16 bind position
  uint8_t b0 = 0, b1 = 0, b2 = 0;
  uint8_t w0 = 0, w1 = 0, w2 = 0;  // 1/64 quanta, w0+w1+w2 == 64 (validated)
};

struct ClampVerdict {
  uint32_t worst_vertex = 0;
  uint32_t worst_frame = 0;
  int32_t worst_err = 0;         // fx16 — w2 * |p3(f) - p12(f)| at the worst frame
  bool warn = false;             //   > 1% of bound radius
  bool reject = false;           //   > 3% of bound radius (author must re-rig)
  uint32_t renorm_adjusted = 0;  // vertices whose largest weight got the
                                 // force-to-64 adjustment
};

/**
 * The compile gate (creature_rules 3, the CORRECTED claim — the drop error
 * is NOT sub-quantum; worst legal case ~13 mm). For every source vertex:
 * renormalize the top-2 weights into 1/64 quanta (round-half-up, sum forced
 * to exactly 64 by adjusting the LARGEST weight), then over EVERY frame of
 * EVERY clip compute the exact drop error
 *   err(f) = w2 * | p3(f) - p12(f) |
 * where p3 = T_b2 applied to the bind vertex and p12 = the renormalized
 * 1-2 blend (the metric the spec names; with exact renormalization it
 * equals |v - v'|, asserted in the tests). Distance via isqrt_u64 on the
 * exact s64 squared norm (one floor sqrt; report form). Warn above 1% of
 * the bound radius, REJECT above 3%. `out` (optional) receives the clamped
 * SkinVertices (the compiled payload) on success.
 */
ClampVerdict clamp_3to2(const std::vector<SourceVertex>& src, const Skeleton& sk,
                        const SkeletonBake& baked, const ClipBank& bank, int32_t bound_radius,
                        std::vector<SkinVertex>* out);

// ------------------------------------------------------------ anim player --

/**
 * The sim-side clip clock (creature_rules 2.1): keys at 30 Hz, each key
 * shown 2 sim ticks — `sub` is the intra-key tick. Hard cuts only: cut()
 * lands the new slot at frame 0 with no blend, ever. Events fire when the
 * displayed frame ENTERS a frame that carries tags (the sim consumes them;
 * the impact chain of creature_rules 4.2 starts here).
 */
struct AnimPlayer {
  uint16_t slot = 0;
  uint16_t frame = 0;
  uint8_t sub = 0;
  bool frozen = false;  // petrify: the clip clock stops (4.2)

  void cut(uint16_t new_slot) {
    slot = new_slot;
    frame = 0;
    sub = 0;
  }
};

/**
 * Advance one sim tick. Returns the events fired THIS tick (view into the
 * clip; valid until the next advance) — fires on the 1->0 sub wrap that
 * enters a tagged frame. A frozen player advances nothing.
 */
void anim_advance(AnimPlayer& a, const ClipBank& bank, const ClipEvent** fired,
                  uint8_t& fired_count);

// -------------------------------------------------------- alive laws (4.2) --

enum class TiltMode : uint8_t {
  kNone = 0,        // bipeds stay upright
  kSideways = 1,    // side slope only (roll)
  kCompletely = 2,  // facing + side slope (pitch and roll)
};

/**
 * rotateOnGround state: the current forward/side slopes (fx16, tan of the
 * pitch/roll angles). Rate-limited toward the tap-derived target by
 * max_step per tick (the integer stand-in for the donor's rate-limited
 * slerp — slope space, stated honestly).
 */
struct GroundTilt {
  int32_t slope_f = 0;  // fx16
  int32_t slope_s = 0;  // fx16
};

/**
 * Two column_query taps along facing + side (creature_rules 4.2: THE
 * cheapest alive trick — 2 taps + a clamp per creature tick). Finite
 * difference over +-tap_dist gives the target slopes; each axis clamps its
 * per-tick change to max_step. kOut/kVoid columns hold the slope (a
 * creature stepping off the island does not snap flat).
 */
void ground_tilt_update(GroundTilt& t, TiltMode mode, angle16 facing,
                        const terrain::ComposedLattice& lat, fx16 x, fx16 z, fx16 tap_dist,
                        fx16 max_step);

/**
 * The tilt rotation: Rodrigues rotation of +Y onto normalize(-sf, 1, -ss)
 * (the ground normal under the slopes). Built in fx16 from the shared
 * normalize (qformats 7.4) + one field_rcp for 1/(1 + n.y); ~30 multiplies,
 * once per creature per frame. Deterministic, integer-only.
 */
mat3x4fx tilt_matrix(const GroundTilt& t, SatLedger* L);

/**
 * Bulk inflation (4.2): one scalar at the root, exponential smoothing
 * toward the target — scale += (target - scale) >> rate_shift per tick
 * (integer exponential; the hand anchor is in the tests). Crossing a
 * species' pop threshold gibs the creature (bulk_popped).
 */
struct BulkState {
  int32_t scale = 1 << 16;   // fx16
  int32_t target = 1 << 16;  // fx16
};

void bulk_update(BulkState& b, uint8_t rate_shift);
inline bool bulk_popped(const BulkState& b, int32_t threshold_fx) {
  return b.scale >= threshold_fx;
}

/**
 * Tick-skip slow-motion (4.2): the entity's sim update runs every 4^n ticks
 * — a modulo counter. n = shift (interval 1 << (2 * shift)); 0 = every tick.
 */
inline bool tick_skip_due(uint32_t tick, uint8_t shift) {
  const uint32_t mask = (1u << (2u * shift)) - 1u;
  return (tick & mask) == 0;
}

// ------------------------------------------------------------- LOD ladder --

enum class LodRung : uint8_t {
  kMesh = 0,
  kMicro = 1,
  kSplat = 2,
  kGlint = 3,
};

/** Charter 9 stability: minimum hold before any rung switch (15 ticks —
 *  the star ladder's precedent, stars_and_flares 6/9). */
inline constexpr uint16_t kLodHoldTicks = 15;

/**
 * Screen-space error law (The Measure, reference form): with the projected
 * bound radius in S12.8 px and rung geometric error e_r (fx16 world),
 *   err_px(S12.8) = round_half_up(proj_radius_q8 * e_r / bound_radius)
 * A rung is LEGAL when err_px <= threshold (the per-camera pixel-error
 * threshold, S12.8). The raw selection takes the COARSEST legal rung
 * (fewest pixels that still meets the error budget).
 */
bool projected_bound_radius_q8(const mat4fx& vp, int32_t world_x, int32_t world_y, int32_t world_z,
                               int32_t bound_radius, uint32_t viewport_w, int32_t& radius_q8,
                               SatLedger* L);

LodRung lod_raw(int32_t proj_radius_q8, int32_t thresh_q8, const CreatureType& type);

/** Ladder state: current rung + ticks held there. */
struct LodState {
  LodRung rung = LodRung::kMesh;
  uint16_t hold = 0;
};

/**
 * The hysteresised step (charter 9: hysteresis + minimum hold; the star
 * ladder's 10%/15-tick law): a switch requires (a) the current rung held
 * >= 15 ticks and (b) the projected radius 10% PAST the boundary between
 * the current and target rung in the target's direction (coarsening is
 * eager, refinement is lazy — rungs do not flip at the seam).
 */
LodRung lod_update(LodState& st, int32_t proj_radius_q8, int32_t thresh_q8,
                   const CreatureType& type);

// ----------------------------------------------------------------- gibs ----

/** A gib particle (the PART.SPAWN burst payload — DrawPopulation shape). */
struct Gib {
  int32_t x, y, z;     // fx16 spawn position (world)
  int32_t vx, vy, vz;  // fx16 per-tick velocity
  uint8_t size;        // U 0.4.4 px
  uint8_t r, g, b;
};

/**
 * Gib-to-particles (creature_rules 4.2: bulk crossing the pop threshold ->
 * mesh removed, particle burst). One gib per meshlet vertex (capped at 64),
 * velocity from noise2_hash (qformats 7.5 — the deterministic integer
 * pattern; no RNG state), colour from the vertex's meshlet material
 * darkened one step. Deterministic: same pose + seed -> same burst.
 */
void spawn_gibs(const CreatureType& type, const mat3x4fx* palette, fx16 wx, fx16 wy, fx16 wz,
                uint32_t seed, std::vector<Gib>& out);

/**
 * The sim tick (the one place the alive laws compose): tick-skip gate
 * (slow-motion: the entity updates every 4^n ticks — a modulo counter),
 * then anim clock advance (fires the frame's event tags), rotateOnGround
 * tilt (two column_query taps against the composed lattice — the caller
 * passes the SAME lattice the renderer tessellates; nullptr holds tilt),
 * bulk smoothing. Returns whether the update ran this tick (the skip
 * gate). Pop handling stays with the caller: bulk_popped() + spawn_gibs()
 * are the pieces (the sim owns gameplay truth, charter 29-8).
 */
struct SimParams {
  uint8_t skip_shift = 0;   // update every 4^n ticks (0 = every tick)
  fx16 tap_dist{2 << 14};   // rotateOnGround tap half-distance (0.5 m)
  fx16 tilt_step{1 << 13};  // per-tick slope rate limit (0.125)
};

struct CreatureInstance;  // defined below (compositor section)

bool creature_update(CreatureInstance& inst, const SimParams& sp,
                     const terrain::ComposedLattice* lat, uint32_t tick);

// ---------------------------------------------------- instance + compositor --

/**
 * One creature instance: sim state (anim clock, tilt, bulk, LOD) + world
 * placement. The pose is NOT stored — it is shared through the PoseBank
 * (that sharing IS the creature_rules 2.2 economy).
 */
struct CreatureInstance {
  const CreatureType* type = nullptr;
  int32_t x = 0, y = 0, z = 0;  // fx16 world root
  angle16 facing{0};
  GroundTilt tilt;
  TiltMode tilt_mode = TiltMode::kCompletely;
  BulkState bulk;
  AnimPlayer anim;
  LodState lod;
  bool visible = true;
  // THE CHOREO ROOT (C1, 2026-08-27; ZixxtrixxV2amendment). When `choreo`
  // is set the instance's world transform is T(x,y,z) · R(orient) ·
  // bulk-scale — a FULL 3D orientation replacing RotY(facing)·tilt, so
  // trajectory, spin count, spin plane and attack direction live on the
  // INSTANCE while shared clips keep only local body shape. The sim owns
  // building `orient` (folding facing in, freezing ground tilt at takeoff);
  // the pose cache still serves shared (type, clip, frame) palettes —
  // nothing about the creature_rules 2.2 economy changes, which is the
  // point (sharing proof: tools/reel/zixx_choreo.cpp). Decode: quat16 →
  // 3×3 is the same frozen 9-product formula GEOM.POSE ratified
  // (qformats 7.6); on the silicon side per-instance transforms are
  // GEOM.LOOM's chartered purpose, and this lane is recorded for its
  // contract. Default false: existing callers unchanged.
  bool choreo = false;
  quat16 orient = quat16_identity();
};

/**
 * The reference compositor preview ([phase3-preview], the same station the
 * celestial compositor holds): draws creatures into the working RGB888 +
 * Q16.16-depth planes AFTER the opaque passes, depth-tested against what is
 * already there (terrain occludes creatures correctly). Mesh/micro rungs
 * skin + project + flat-shade through zrender's own raster (the 8 law —
 * never reimplemented); splat is a billboard quad at the projected bound;
 * glint is a 2 px point. Bulk scale multiplies the world transform at the
 * root (one scalar). Integer-only; doubles appear nowhere.
 *
 * This is the ORACLE surface for the Phase 8-9 render path, not a shipping
 * ABI command — the DrawCreature-class opcode question lands with the ABI
 * owner when the block contracts fill.
 */
void compose_creatures(uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h, const mat4fx& vp,
                       CreatureInstance* const* instances, size_t count, PoseBank& poses,
                       SatLedger* L);

/**
 * THE ATTACK PLAN (C4/C5, 2026-08-28; the programmable-salto amendment).
 *
 * "Shared clips own local body shape; a per-instance full-3D root transform
 * owns trajectory, spin count, spin plane and attack direction. The sim
 * builds an AttackPlan and branches on actual collision."
 *
 * The plan is a pure fixed-point record built by the SIM at the trigger
 * tick from sim truth (target position/velocity, terrain). Everything is
 * integer: mm, 30 Hz key counts, 1/1000 turns. Replay from a capture
 * reproduces plan, trajectory and branch bit-exactly. The COMMITMENT RULE:
 * limited aim correction is allowed while coiled; the spear vector LOCKS at
 * unroll -- from spear commit onward the creature is a projectile, not a
 * missile (the counterplay).
 */
struct AttackPlan {
  bool preset_golden = false;  // the approved Ground Dive choreography
  // phase durations, 30 Hz keys. Direction #9 makes maximum compression a
  // first-class hold instead of hiding it inside a wobble-shaped clip. The
  // shared whole-S release is four keys: collapsed at 18, assembled at 19,
  // a support-compensated bridge at 20, absorb at 21, exact grounded at 22
  // before the rigid lift.
  // NOTE (RUN-20260902-1816): these engine-side defaults still read a
  // long-rejected schedule. Harmless only because zixx_plan_attack and
  // zixx::JumpPlan overwrite all three from the kSalto* timeline -- do NOT
  // default-construct an AttackPlan and expect the accepted timing.
  uint16_t compress_keys = 12, compress_hold_keys = 6, release_keys = 4;
  uint16_t coil_keys = 20, unroll_keys = 9, plunge_keys = 10;
  // trajectory
  int32_t apex_mm = 0;      // root lift at the top of the coil flight
  int32_t apex_fwd_mm = 0;  // forward travel by the apex
  int32_t spin_mturns = 0;  // total coil spin, 1/1000 turns (the ROOT's)
  // the committed spear: from the commit point (apex) toward the intercept
  int32_t spear_dx_mm = 0, spear_dy_mm = 0;
  // sim expectation (the BRANCH still comes from actual collision -- C5)
  int32_t intercept_x_mm = 0, intercept_y_mm = 0;
};

/** C5: what actually happened at the end of the committed spear path. */
enum class AttackOutcome : uint8_t { kGroundStick, kAirHit, kMissRecover };

/**
 * THE BRANCH LAW (C5): impact is a COLLISION VERDICT, never a clip key.
 * The sim reports what the committed path actually met -- terrain, a
 * creature, or nothing -- and the choreography player cuts to the matching
 * phase clip. An airborne Zixxtrixx can no longer play a ground-impact
 * event in empty sky because key 62 arrived.
 */
inline AttackOutcome attack_plan_branch(bool hit_terrain, bool hit_creature) {
  if (hit_terrain) return AttackOutcome::kGroundStick;
  if (hit_creature) return AttackOutcome::kAirHit;
  return AttackOutcome::kMissRecover;
}

/**
 * DIAGNOSTIC shade modes for the acceptance gate (P2, 2026-08-28): unlit
 * (fullbright / texture-only), packed-normal visualisation, and wireframe.
 * A process-global because it is REEL TOOLING, not part of the oracle
 * surface: kOff (the default) is bit-identical to not having the enum at
 * all, no pinned subject sets it, and no RTL contract sees it.
 */
enum class DebugShade : uint8_t { kOff = 0, kUnlit, kNormals, kWire, kTriangleIds };
extern DebugShade g_debug_shade;

/**
 * Named, editable world-space preview-light controls. The baseline remains the
 * default oracle behavior; reel tooling may point g_creature_light_rig at one
 * of the alternatives for an identical-art comparison. These values affect
 * illumination only: no material, toon-ramp, geometry or camera state lives in
 * a rig. Directions and gains are Q16.16.
 */
struct CreatureLightRig {
  int32_t key_x, key_y, key_z;
  int32_t fill_x, fill_y, fill_z;
  int32_t ambient_r, ambient_g, ambient_b;
  int32_t key_gain;
  int32_t fill_r, fill_g, fill_b;
};
extern const CreatureLightRig kCreatureLightBaseline;
extern const CreatureLightRig kCreatureLightFrontSoft;
extern const CreatureLightRig kCreatureLightHighOpen;
extern const CreatureLightRig kCreatureLightCrossfill;
// V12 experimental owner-choice rigs. Exactly ten new options, each with a
// dominant world-space key from above; the v11 declarations above remain
// byte-identical references and are not counted among these ten.
extern const CreatureLightRig kCreatureLightZenithSun;
extern const CreatureLightRig kCreatureLightMorningCrown;
extern const CreatureLightRig kCreatureLightEveningCrown;
extern const CreatureLightRig kCreatureLightNorthSkylight;
extern const CreatureLightRig kCreatureLightSouthSkylight;
extern const CreatureLightRig kCreatureLightOpenOvercast;
extern const CreatureLightRig kCreatureLightHardNoon;
extern const CreatureLightRig kCreatureLightVeiledSun;
extern const CreatureLightRig kCreatureLightSilverMoon;
extern const CreatureLightRig kCreatureLightCloudbreak;
// V13: exactly one post-diagnosis candidate. It is separate from the rejected
// v12 family and is evaluated only after the generic outward-normal repair.
extern const CreatureLightRig kCreatureLightCorrectedToplight1;
// V14: exactly ten artistic top-diagonal modes. All preserve v13's corrected
// surface-to-source/outward-normal convention and differ only in named light
// controls; the held pose, materials and view-only orbit remain common.
extern const CreatureLightRig kCreatureLightDiagonalDaylight;
extern const CreatureLightRig kCreatureLightDiagonalOpenSky;
extern const CreatureLightRig kCreatureLightDiagonalWarmCross;
extern const CreatureLightRig kCreatureLightDiagonalCoolCross;
extern const CreatureLightRig kCreatureLightDiagonalSoftCloud;
extern const CreatureLightRig kCreatureLightDiagonalHardSun;
extern const CreatureLightRig kCreatureLightDiagonalCloudbreak;
extern const CreatureLightRig kCreatureLightDiagonalSilverMoon;
extern const CreatureLightRig kCreatureLightDiagonalBroadBounce;
extern const CreatureLightRig kCreatureLightDiagonalRoseDusk;
// Final Zixxtrixx inspection clip: deliberately dim directional daylight so a
// small moving local source can be read without changing the creature material.
extern const CreatureLightRig kCreatureLightMovingInspection;
extern const CreatureLightRig* g_creature_light_rig;

/**
 * Optional reel-inspection point sources. Position and falloff radii are raw
 * Q16.16 world coordinates; RGB gains are Q16.16. The globals name a
 * contiguous ARRAY of descriptors and its count; a zero count (the shipping
 * default) takes the exact pre-point-light code path and leaves
 * compose_creatures byte-identical. Reel tooling samples the world-space
 * positions per frame and points these globals at those SAME descriptors
 * while composing and drawing the visible markers. Each source carries its
 * own RGB gain, and overlapping pools SUM per channel before the shade
 * quantiser -- linear transport, so intersecting coloured pools mix.
 * Direction 26 sized the array for the moving-light inspection's four
 * sources; kCreatureMaxPointLights only bounds reel-side storage, the
 * compositor walks whatever count it is handed.
 */
struct CreaturePointLight {
  int32_t world_x, world_y, world_z;
  int32_t inner_radius, outer_radius;
  int32_t gain_r, gain_g, gain_b;
};
constexpr uint32_t kCreatureMaxPointLights = 4;
extern const CreaturePointLight* g_creature_point_lights;
extern uint32_t g_creature_point_light_count;

// RUN 1939/2234 texture-experiment lane. 0 = off (the shipping path,
// bit-identical). g_cel_bands 2/3 selects constant-per-triangle FACETED cel.
// g_smooth_toon_bands 2/3 keeps coherent shared normals/Gouraud light and asks
// the raster to threshold the interpolated light per fragment. They are two
// explicit style reads, never simultaneous and never part of the asset.
extern int g_cel_bands;
extern int g_smooth_toon_bands;

/* ---------------------------------------------------------------------------
 * THE CREATURE EXTENT LAW (owner ruling, docs/OWNER_DOCKET.md 2026-08-24 item 3)
 * ---------------------------------------------------------------------------
 * What this buys, and why it is an ASSET law rather than a hardware one:
 *
 * GEOM.SKIN and GEOM.MAT3X4_MUL contain NO world coordinates at all. Their
 * multiplier operands are palette 3x3 entries times a MODEL-space vertex, and
 * the world position enters only through the additive translation column. So
 * their operands are bounded by the CREATURE, not by the island -- but nothing
 * said how big a creature may be, so nothing could prove the bound.
 *
 * The ruling supplies the missing content law. Once assets are validated to it,
 * both blocks 3x3 terms are rigid rotation coefficients and their vertex
 * operands are bounded, which is the precondition for narrowing them. It does
 * NOT by itself bank any DSP saving: each block must still map and show BOTH
 * operands land in the cheap band.
 *
 * WHAT IS GIVEN UP: skeletal shear, per-bone non-uniform squash/stretch, and a
 * single ordinary skinned mesh beyond ~1 km at maximum scale. Growth, size
 * variation and giants all survive -- giants are a separate terrain-patch path,
 * and uniform SCALE stays an explicit Loom operation rather than being folded
 * into the bone matrices.
 */

/** 128 m in fx16 raw. Every posed vertex of every clip frame must lie within
 *  this radius of the root BEFORE Loom scale and world translation. */
constexpr int32_t kCreatureLocalRadius = 128 * 65536;

/** Cumulative uniform Loom scale affecting a rendered part. Fixed authoring
 *  scale is baked offline, so this bounds RUNTIME scale only. */
constexpr int32_t kMaxCumulativeScale = 4 * 65536;

/** How a bone matrix may fail the rigid test. Distinguished rather than
 *  collapsed into one bool: "your rig has shear" and "your rig has scale baked
 *  into a bone" are different notes to send an author. */
enum class RigidFault : uint8_t {
  kNone = 0,
  kRowNormNotUnit,    // a 3x3 row is not unit length: scale baked into the bone
  kRowsNotOrthogonal  // two 3x3 rows are not perpendicular: shear
};

struct ExtentVerdict {
  uint32_t worst_vertex = 0;
  uint32_t worst_frame = 0;
  int32_t worst_radius = 0;    // fx16, the largest |posed vertex - root|
  bool radius_reject = false;  // > kCreatureLocalRadius
  uint16_t rigid_fault_bone = 0;
  RigidFault rigid_fault = RigidFault::kNone;
  bool scale_reject = false;  // cumulative scale outside (0, 4]
  int32_t worst_scale = 0;

  bool ok() const { return !radius_reject && rigid_fault == RigidFault::kNone && !scale_reject; }
};

/**
 * Is a 3x3 block a rotation -- no scale, no shear? The tolerance is the
 * ratified quaternion-to-matrix norm drift (spec/qformats.md 412-414), NOT an
 * arbitrary epsilon: a matrix that came from a legal unit quaternion must pass,
 * and the drift bound is exactly how far one can legally sit from unit length.
 */
RigidFault rigid_fault_of(const mat3x4fx& m, uint32_t tol_q16 = 16);

/**
 * The compile gate for the extent law. Over EVERY vertex of EVERY frame of
 * EVERY clip -- the same total sweep clamp_3to2 uses, and for the same reason:
 * a bound that holds on the bind pose says nothing about a reaching animation.
 *
 * REJECTS rather than clamps, matching TERRAIN.BAKE radius and SetView. A
 * clamped creature is a creature the author did not author.
 */
ExtentVerdict validate_extent(const std::vector<SourceVertex>& src, const Skeleton& sk,
                              const SkeletonBake& baked, const ClipBank& bank,
                              int32_t cumulative_scale);

}  // namespace creature
}  // namespace zref
