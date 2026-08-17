// zref_render.hpp — W3.5 ZRef software renderer (plan W3.5 / ratified D7:
// "zref::render — exact, slow, integer-only, CRC-able").
//
// The renderer consumes a VALIDATED sealed frame packet (zhao execution
// result with the command stream, spec/capture_format.md §3) plus a 2-slot
// RGB565 canvas and renders the Phase-3 subset of ABI commands:
//
//   SetView 0x0010, SetPresentationContract 0x0020 — per-view state, mat4fx
//       projection (zref::mat4_vec4), viewport maps per video_rules.md;
//   DrawProcedural 0x0302 (forge_kind = heightfield_patch) — the island;
//   TerrainField 0x0200 — earth .zprog over the footprint via the ONE zfield
//       interpreter (never reimplemented here);
//   SurfaceStamp 0x0210 — circle/ring into the 64x64 surface sheet (the scar
//       response), persistent across frames;
//   DrawForm 0x0300 — marker/billboard quads (the wizards);
//   DrawPopulation 0x0301 — point/triangle particle sprites;
//   DrawSky 0x0310 — through zref::sky::emit_layers (spec/sky_and_beams.md);
//   EmitAudioEvent 0x0400 — tone triggers for the caller's zref::MixerTone.
//
// Law (in citation order):
//   spec/qformats.md   §2 mat4fx x vec4 (s128 exact row sum, ONE rescale),
//                      fx16/height16 conversions
//                      §3 single-rounding law (fx_mul/fx_mad/fx_div_exact)
//                      §8 screenXY S 12.8 + ±2048 guard band, edge functions
//                         (s64 setup, E' = E0>>8, top-left bias), depth law
//   spec/video_rules.md §1 modes + mode latch law (frame start only)
//                      §3 framebuffer layout (2 slots, RGB565 LE, slot size
//                         = largest canvas), §3.1 Duo canvas map
//                      §4 displayed-CRC law (raster order, border rows)
//   charter §8         pass order inside a frame (the renderer's render
//                      order: prefill -> under-plane -> opaque terrain+forms
//                      -> translucent sky -> particles -> resolve)
//   charter §12        surface sheet: 64x64 texels, 8-bit tag + 8-bit
//                      strength, per-patch, persistent; stamps deterministic
//   spec/capture_format.md §3 sealed frame packet + fail-safe validation
//   spec/form/field-ir.md §7.1 earth I/O record (x, z, age, phase, p0..p7 ->
//                      height, velocity, material, nav_cost)
//   spec/audio_rules.md §4 frozen tone table (the EmitAudioEvent consumer)
//   spec/cartridge.md  §4 page families (terrain patch kind 4 shape below)
//   spec/sky_and_beams.md — via zref_sky.hpp
//
// NO floats anywhere in the render path (plan W3.5; grep-audited by
// tests/render). Every numeric helper lives in zref:: (qformats.md is the
// single numeric law — nothing is re-derived here).

#pragma once

#include "zhao_abi.h"  // generated: opcodes, video_mode, record structs

#include "zref/zref_audio.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_sat.hpp"
#include "zref/zref_sky.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace zfield {
struct Decoded;  // reference/include/zfield/zfield.hpp (called, never re-done)
}

namespace zref {
namespace render {

// ---------------------------------------------------------------- canvas ----

// video_rules.md §1/§3: ALLOCATION, not occupancy — two slots, each sized for
// the LARGEST canvas (0x3C000 = 245,760 B) so a mode switch never moves a
// slot. What a frame OCCUPIES is canvas_bytes(mode) below.
inline constexpr uint32_t kSlotBytes = 0x3C000;

/** Duo: one view canvas = 256x192x2 = 0x18000 B (video_rules.md §3.1). */
inline constexpr uint32_t kDuoViewBytes = 0x18000;

/**
 * Canvas bytes a frame OCCUPIES per mode (video_rules.md §1/§3.1).
 *
 * Duo stores its two views as CONTIGUOUS PACKED BLOCKS — view 0 at slot bytes
 * [0, 0x18000), view 1 at [0x18000, 0x30000) — NOT interleaved side-by-side
 * every 256 pixels of a 512-wide image (ratified 2026-08-15, review MAJOR-3:
 * the scanout fetcher reads one linear burst stream per view). The 48 black
 * border rows are scanout-side: never stored, but part of the displayed CRC.
 */
inline constexpr uint32_t canvas_bytes(zhao_abi::video_mode m) {
  switch (m) {
    case zhao_abi::VIDEO_Z60:
      return 184320u;  // 384x240x2
    case zhao_abi::VIDEO_STORM:
      return 153600u;  // 320x240x2
    case zhao_abi::VIDEO_DUO:
      return 196608u;  // 0x30000 = 2 x 256x192x2, packed blocks (§3.1)
  }
  return 0;
}

/**
 * STORAGE raster of the canvas a mode renders into (§1/§3.1) — the shape of
 * the byte image in the slot, which for Duo is the two view blocks STACKED
 * (256 wide, 384 tall: view 0 rows 0..191, view 1 rows 192..383), so a plain
 * row-major resolve emits exactly the packed layout above. The DISPLAYED
 * raster is 512x240 and is assembled at scanout (displayed_crc32c).
 */
inline constexpr uint32_t canvas_width(zhao_abi::video_mode m) {
  return m == zhao_abi::VIDEO_DUO ? 256u : (m == zhao_abi::VIDEO_Z60 ? 384u : 320u);
}
inline constexpr uint32_t canvas_height(zhao_abi::video_mode m) {
  return m == zhao_abi::VIDEO_DUO ? 384u : 240u;  // Duo = 2 stacked view blocks
}

/** The 2-slot RGB565 canvas (video_rules.md §3). Row-major, LE halfwords. */
struct RenderCanvas {
  std::vector<uint8_t> slot[2];
  RenderCanvas() {
    slot[0].assign(kSlotBytes, 0);
    slot[1].assign(kSlotBytes, 0);
  }
};

// ------------------------------------------------------------- resources ----

/**
 * Authored heightfield patch — the terrain-patch page body (spec/cartridge.md
 * §4 kind 4): header extents + width x height height16 samples. Heights are
 * s16 S 1.7.8 raw (qformats §9); world positions come from the envelope.
 *
 * [world-identity wave] The Island Patch v1 layers (spec/terrain_rules.md §2)
 * ride along in-memory: scar (B), bottom (C), cell_state (D). All three empty
 * = the Phase-3 legacy single-surface page (terrain_rules §3.1 option (a),
 * kept as the degenerate case): every cell SOLID, no underside, no rim walls,
 * pixel-identical to the pre-migration renderer. `dual()` requires bottom to
 * match the lattice extent; a mismatched layer is ignored (fail-safe: a
 * malformed page never changes geometry that did draw). The kind-6 page BYTE
 * layout / pitch_log2 addressing / sparse island directory are Phase-6
 * loader work — this struct is the envelope-based Phase-3 resource.
 */
struct TerrainPatch {
  uint16_t width = 0;
  uint16_t height = 0;
  int32_t env_x0 = 0, env_z0 = 0, env_x1 = 0, env_z1 = 0;  // rectfx raw fx16
  std::vector<int16_t> heights;                            // ascending z-then-x (cartridge.md §4)
  std::vector<int16_t> scar;        // layer B: persistent bake delta (height16); empty = zero
  std::vector<int16_t> bottom;      // layer C: the island underside (height16); empty = legacy
  std::vector<uint8_t> cell_state;  // layer D: (width-1)*(height-1) substance+flags bytes
  // [deep-keel wave] the texturing layers (terrain_rules §2 E/H + §6):
  // mat_a/mat_b/mat_w per cell ((width-1)*(height-1)); tint per vertex
  // (width*height, RGB565 LE). ALL EMPTY = the Phase-3 legacy flat path,
  // pixel-identical to the pre-texturing renderer (goldens pin it).
  std::vector<uint8_t> mat_a;  // layer E candidate A (tile id)
  std::vector<uint8_t> mat_b;  // layer E candidate B (tile id)
  std::vector<uint8_t> mat_w;  // layer E weight (unit8; 255 = pure A)
  std::vector<uint16_t> tint;  // layer H per-vertex RGB565 (LMAP heir)
  uint32_t tileset_id = 0;     // §2.1 header field; 0 = no tileset
  bool dual() const { return bottom.size() == static_cast<size_t>(width) * height; }
  bool textured() const {
    return tileset_id != 0 && mat_a.size() == static_cast<size_t>(width - 1) * (height - 1);
  }
};

/**
 * A terrain tileset (terrain_rules §6.1): 256 CLUT8 tiles of 64x64 + one
 * shared RGB565 palette. Ids 240 (rim strata) and 241 (underside) are the
 * frozen reserved assignments (§6.6). Reference-side asset container; the
 * VRAM layout/swizzle is Phase-6 TEXTURE.CACHE work.
 */
struct Tileset {
  uint16_t palette[256] = {};        // RGB565 LE halfwords
  uint8_t tiles[256][64 * 64] = {};  // CLUT8 indices per tile
};

/** DrawProcedural material page (Phase-3 subset: a flat base colour). */
struct Material {
  uint8_t r = 128, g = 128, b = 128;
};

/**
 * DrawForm marker page — the fixed 8x8 pattern (plan W3.5: "fixed 8x8
 * pattern scaled, wall-clamped per the demo law lineage"). 64 texels of
 * RGB888 plus a 1-bit colour key: mask 0 = transparent (pattern pixel is
 * skipped), mask 1 = opaque.
 */
struct FormPattern {
  uint8_t rgb[64 * 3] = {};
  uint8_t mask[64] = {};
};

/** DrawForm transform resource: world placement + size (both fx16 raw). */
struct FormTransform {
  int32_t x = 0, y = 0, z = 0;  // world3 centre
  int32_t size = 1 << 16;       // fx16 half-extent (world m or screen px)
};

/** DrawPopulation particle (pool SoA snapshot; qformats §10 spirit). */
struct Particle {
  int32_t x = 0, y = 0, z = 0;  // world3 fx16 raw
  uint8_t size = 16;            // U 0.4.4 px (side length), qformats §10
  uint8_t r = 255, g = 255, b = 255;
};

struct Population {
  std::vector<Particle> parts;
};

/** Tone bank entry (cartridge.md §4 kind 5 record shape). */
struct ToneBankEntry {
  uint32_t event_id = 0;
  uint16_t gain = 0;
  int16_t pan = 0;
  uint32_t sample_index = 0;  // 0..2 -> audio_rules.md §4 frozen tone table
};

/**
 * Host-supplied resource tables (W3.6 ZEmu fills these from the .zpak pages;
 * tests build them by hand). Lookup is by the FULL handle value the command
 * carried (handle32 {index:24, generation:8}) — linear, deterministic.
 * A handle that resolves to nothing is counted in RenderResult::resource_misses
 * and the command is skipped: a missing asset never changes geometry that
 * did draw (fail-safe, capture_format.md §3 spirit).
 */
struct RenderResources {
  std::vector<std::pair<uint32_t, const zfield::Decoded*>> field_programs;
  std::vector<std::pair<uint32_t, const TerrainPatch*>> terrain_patches;
  std::vector<std::pair<uint32_t, Material>> materials;
  std::vector<std::pair<uint32_t, FormPattern>> forms;
  std::vector<std::pair<uint32_t, FormTransform>> transforms;
  std::vector<std::pair<uint32_t, Population>> populations;
  std::vector<std::pair<uint32_t, sky::SkySet>> sky_sets;
  std::vector<std::pair<uint32_t, ToneBankEntry>> tones;
  std::vector<std::pair<uint32_t, Tileset>> tilesets;

  const zfield::Decoded* field_program(uint32_t h) const;
  const TerrainPatch* terrain_patch(uint32_t h) const;
  const Material* material(uint32_t h) const;
  const FormPattern* form(uint32_t h) const;
  const FormTransform* transform(uint32_t h) const;
  const Population* population(uint32_t h) const;
  const sky::SkySet* sky_set(uint32_t h) const;
  const ToneBankEntry* tone(uint32_t h) const;
  const Tileset* tileset(uint32_t h) const;
};

// ----------------------------------------------------------- surface sheet --

/**
 * The per-patch surface sheet (charter §12: 64x64 texels, 8-bit tag + 8-bit
 * strength). Persistent across frames inside the renderer; stamps are
 * deterministic inputs (charter §12 "stamps are deterministic commands").
 */
struct SurfaceSheet {
  uint8_t tag[64 * 64] = {};
  uint8_t strength[64 * 64] = {};
};

// ----------------------------------------------------------------- results --

/** EmitAudioEvent -> tone trigger (spec/audio_rules.md lane; the caller —
 *  ZEmu — feeds zref::MixerTone; fire-and-forget, FORM §15). */
struct AudioEventTrigger {
  uint32_t event_id = 0;
  int16_t pan_fx = 0;
  uint16_t gain = 0;
  uint32_t sample_handle = 0;
  uint32_t timestamp = 0;
};

/** TerrainField velocity output lane, recorded per evaluated column. */
struct TerrainVelocitySample {
  int32_t world_x = 0;   // fx16 raw
  int32_t world_z = 0;   // fx16 raw
  int32_t velocity = 0;  // fx16 raw (field-ir.md §7.1 earth out lane 2)
};

struct RenderResult {
  uint8_t status = 0;  // zhao_abi_error: validation status of the packet
  uint32_t frame_id = 0;
  uint32_t canvas_crc32c = 0;     // CRC-32C over the canvas bytes written
  uint32_t displayed_crc32c = 0;  // video_rules.md §4 displayed-stream law
  uint32_t commands_executed = 0;
  uint32_t resource_misses = 0;
  std::vector<AudioEventTrigger> audio_events;
  std::vector<TerrainVelocitySample> terrain_velocity;
  SatLedger sat;  // the frame's overflow observability (qformats §5)
};

// -------------------------------------------------------------- renderer ----

/**
 * The Phase-3 software console renderer (plan W3.5). One instance owns the
 * persistent per-patch surface sheets and the video-mode latch
 * (video_rules.md §1.1: the mode written by SetPresentationContract becomes
 * effective at the NEXT frame start — never mid-frame).
 *
 * Render order inside a frame is the charter §8 pass order; see
 * reference/src/zrender/render_frame.cpp for the pass table.
 */
class SoftwareRenderer {
 public:
  SoftwareRenderer() = default;

  /**
   * Validate (fail-safe, capture_format.md §3.2) + render one sealed frame
   * packet into canvas.slot[dst_slot]. On a validation error nothing is
   * drawn and status carries the error code.
   */
  RenderResult render_frame(const uint8_t* pkt, size_t len, uint32_t dst_slot, RenderCanvas& canvas,
                            const RenderResources& res);
  RenderResult render_frame(const std::vector<uint8_t>& pkt, uint32_t dst_slot,
                            RenderCanvas& canvas, const RenderResources& res);

  /** Clear persistent state (sheets, latched mode -> VIDEO_Z60 reset, §1.1). */
  void reset();

  /**
   * [phase3-preview] pre-resolve hook (spec/stars_and_flares.md §9/§12):
   * invoked once per frame after every pass, immediately before the §8
   * resolve, with the RGB888 working canvas + Q16.16 1/w depth plane
   * (charter §8 tile store) and the frame tick. The celestial compositor
   * preview (zref::star::compose_view) rides this until the reserved
   * `SetCelestials 0x0320` lands in spec/commands.zidl. Plain function
   * pointer + context (no std::function: charter §20.1 flat-C style).
   * fn == nullptr (the default) disables it — nothing changes for any
   * existing consumer or golden.
   */
  using PreResolveFn = void (*)(void* ctx, uint8_t* rgb888, int32_t* depth, uint32_t w, uint32_t h,
                                uint32_t tick);
  void set_pre_resolve(PreResolveFn fn, void* ctx) {
    pre_resolve_ = fn;
    pre_ctx_ = ctx;
  }

  zhao_abi::video_mode latched_mode() const { return mode_latched_; }

  /** Persistent scar state (charter §12) — test/inspection hook. */
  const std::vector<std::pair<uint32_t, SurfaceSheet>>& sheets() const { return sheets_; }
  SurfaceSheet& sheet_for(uint32_t patch_handle);  // creates on first use

 private:
  // video_rules.md §1.1: SetPresentationContract writes the register; the
  // LATCH (used for the frame) happens at frame start — render_frame reads
  // mode_latched_ first, then the walk may rewrite it for the next frame.
  zhao_abi::video_mode mode_latched_ = zhao_abi::VIDEO_Z60;
  std::vector<std::pair<uint32_t, SurfaceSheet>> sheets_;
  PreResolveFn pre_resolve_ = nullptr;  // [phase3-preview] celestial hook
  void* pre_ctx_ = nullptr;
};

// ------------------------------------------------------- resolve + CRC laws --

/**
 * Charter §8 resolve: high-quality ORDERED DITHER on the RGB565 resolve.
 * 4x4 Bayer, thresholds (B+0.5)/16 — the fixgen family has NO dither table
 * today (checked W3.5), so the matrix is defined here, once; when Phase-4
 * freezes the RTL resolve matrix this is the single point to regenerate.
 */
void resolve_rgb565(const uint8_t* rgb888, uint32_t width, uint32_t height,
                    uint8_t* out565);  // out must hold width*height*2 bytes

/** CRC-32C over exactly the canvas bytes of a mode (zhao_crc32c, §3 layout). */
uint32_t canvas_crc32c(zhao_abi::video_mode mode, const uint8_t* slot);

/**
 * The displayed-stream CRC (video_rules.md §4): CRC-32C over exactly
 * 2 x active_width x 240 bytes in raster order, AFTER the repeat decision —
 * for Duo this includes the 48 border rows (black, §3.1) around the two
 * 256x192 view canvases. zref::framePixelCrc from W2.2 was checked for and
 * does NOT exist; this implements that law (noted to W3.8).
 */
uint32_t displayed_crc32c(zhao_abi::video_mode mode, const uint8_t* slot);

// ------------------------------------------------------------ audio helper --

/** sample_index -> the frozen Phase-2 tone (audio_rules.md §4 table). */
bool tone_id_for(uint32_t sample_index, ToneId* out);

}  // namespace render
}  // namespace zref
