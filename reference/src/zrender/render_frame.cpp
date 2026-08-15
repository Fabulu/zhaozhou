// render_frame.cpp — the W3.5 software console frame loop: validate a sealed
// packet, walk the command stream, execute the charter §8 pass order per
// view, resolve to RGB565, CRC the canvas.
//
// Law:
//   spec/capture_format.md §3  sealed frame packet; validation runs FIRST
//                              (the fail-safe order, zhao_frame_validate) —
//                              the renderer never draws an unvalidated packet
//   spec/video_rules.md   §1.1 mode latch law (effective at frame start,
//                              never mid-frame), §3.1 Duo canvas map,
//                              §4 displayed-CRC law
//   charter §8            pass order inside a frame — the render order here:
//                              1 backdrop prefill  (sky bands + cap, or the
//                                fallback flat clear, sky_and_beams §1)
//                              3 opaque geometry   (sky under-plane at real
//                                depth, then heightfield patches, then
//                                DrawForm markers — painter + Q16.16 1/z)
//                              6 translucent       (sun BEFORE cloud, the
//                                sky spec's deterministic sub-order — the
//                                primitive list is emitted cloud-first, so
//                                the sky raster sweeps it twice to honour it)
//                              7 particles         (DrawPopulation, test-only)
//                              9 resolve           (ordered dither RGB565 +
//                                canvas CRC, resolve.cpp)
//                            passes 2/4/5/8 have no Phase-3 members (depth-
//                            only occluders, alpha-test, polygon decals,
//                            foreground plane — later phases).
//   spec/commands.zidl    the six D7 promotions + SetView/SetPresentation-
//                          Contract/DrawSky execution readings below
//   spec/sky_and_beams.md §1 (fallback flat clear; "no pixel is ever left
//                          unwritten" — the prefill clear IS the fallback),
//                          §1.1 layer table depths (bands/cap write the far
//                          constant; under/cloud/sun carry the layer-row
//                          1/z: 65536/2560, /1792, /2560 — [w3.5-software]:
//                          the rotation-only rot_proj keeps w = 1, so the
//                          drum's real-world depth is pinned from the layer
//                          table row instead of an interpolated 1/w)
//
// DrawSky note: 0x0310 is RESERVED in the .zidl (RTL activation is wave 8)
// but the packet validates structurally, and spec/sky_and_beams.md §6 names
// this software path as the Phase-3 consumer of emit_layers — the software
// console executes it (recorded as a W3.5 deviation for W3.8's ABI notes).

#include "internal.hpp"

#include "zfield/zfield.hpp"
#include "zref/zref_frame.hpp"

#include <algorithm>
#include <cstring>

namespace zref {
namespace render {

// ----------------------------------------------------------- resource lookups

namespace {
// value tables: return a pointer to the stored value
template <typename T>
const T* find(const std::vector<std::pair<uint32_t, T>>& tab, uint32_t h) {
  for (const auto& e : tab)
    if (e.first == h) return &e.second;
  return nullptr;
}
// pointer tables: return the stored pointer itself
template <typename T>
T find_ptr(const std::vector<std::pair<uint32_t, T>>& tab, uint32_t h) {
  for (const auto& e : tab)
    if (e.first == h) return e.second;
  return nullptr;
}
}  // namespace

const zfield::Decoded* RenderResources::field_program(uint32_t h) const {
  return find_ptr(field_programs, h);
}
const TerrainPatch* RenderResources::terrain_patch(uint32_t h) const {
  return find_ptr(terrain_patches, h);
}
const Material* RenderResources::material(uint32_t h) const { return find(materials, h); }
const FormPattern* RenderResources::form(uint32_t h) const { return find(forms, h); }
const FormTransform* RenderResources::transform(uint32_t h) const { return find(transforms, h); }
const Population* RenderResources::population(uint32_t h) const { return find(populations, h); }
const sky::SkySet* RenderResources::sky_set(uint32_t h) const { return find(sky_sets, h); }
const ToneBankEntry* RenderResources::tone(uint32_t h) const { return find(tones, h); }

// ------------------------------------------------------------ renderer state

SurfaceSheet& SoftwareRenderer::sheet_for(uint32_t patch_handle) {
  for (auto& e : sheets_)
    if (e.first == patch_handle) return e.second;
  sheets_.emplace_back(patch_handle, SurfaceSheet{});
  return sheets_.back().second;
}

void SoftwareRenderer::reset() {
  mode_latched_ = zhao_abi::VIDEO_Z60;
  sheets_.clear();
}

RenderResult SoftwareRenderer::render_frame(const std::vector<uint8_t>& pkt, uint32_t dst_slot,
                                            RenderCanvas& canvas, const RenderResources& res) {
  return render_frame(pkt.data(), pkt.size(), dst_slot, canvas, res);
}

// sky layer depths (§1.1 layer rows, [w3.5-software] — see file header):
// 1/z of the layer's z distance, floor, Q16.16, EXCEPT the sun at 26 (the
// strict-greater depth test needs the tie-break so the additive sun survives
// the under-plane's depth write — both sit at layer z 2560).
inline constexpr int32_t kSkyUnderDepth = (1 << 16) / 2560;    // 25
inline constexpr int32_t kSkyCloudDepth = (1 << 16) / 1792;    // 36
inline constexpr int32_t kSkySunDepth = (1 << 16) / 2560 + 1;  // 26
inline constexpr int32_t kSkyFarDepth = 0;                     // bands/cap write

RenderResult SoftwareRenderer::render_frame(const uint8_t* pkt, size_t len, uint32_t dst_slot,
                                            RenderCanvas& canvas, const RenderResources& res) {
  RenderResult rr;
  if (dst_slot > 1) {
    rr.status = zhao_abi::ZH_ABI_BAD_VALUE;
    return rr;
  }

  // 1. fail-safe validation first (capture_format.md §3.2)
  const zhao::ZhaoValidateResult v = zhao::zhao_frame_validate(pkt, len);
  rr.status = static_cast<uint8_t>(v.error);
  if (v.error != zhao_abi::ZH_ABI_OK) return rr;

  // 2. mode latch: this frame renders under the LATCHED mode; a contract in
  // this frame becomes effective next frame start (video_rules.md §1.1)
  const zhao_abi::video_mode mode = mode_latched_;

  const zhao::ZhaoFrameHeader hd = zhao::zhao_frame_parse_header(pkt);
  rr.frame_id = hd.frame_id;

  // ---- per-frame draw state ----------------------------------------------
  struct ViewSt {
    bool active = false;
    mat4fx vp;
    int32_t pixel_error = 0;
  } views[2];
  std::vector<FieldApp> fields;
  struct TerrainInst {
    const TerrainPatch* patch;
    uint32_t patch_handle;
    ZhTransform2fx xform;
    const Material* mat;
  };
  std::vector<TerrainInst> terrain;
  struct FormDraw {
    const FormPattern* form;
    const FormTransform* xf;
    uint8_t viewport_mask;
    uint16_t flags;
  };
  std::vector<FormDraw> forms;
  struct PopDraw {
    const Population* pop;
    uint8_t viewport_mask;
    uint16_t flags;
  };
  std::vector<PopDraw> pops;
  struct SkyDraw {
    const sky::SkySet* set;
    zhao_abi::ZhRecordDrawSky cmd;
  };
  bool has_sky = false;
  SkyDraw sky{nullptr, zhao_abi::ZhRecordDrawSky{}};
  uint32_t tick = hd.frame_id;

  // ---- walk the command stream (validation guarantees structure) ---------
  zhao_abi::ZhReader r(pkt + zhao_abi::ZHAO_FRAME_HEADER_BYTES, hd.command_bytes);
  for (uint32_t ci = 0; ci < hd.command_count; ++ci) {
    // peek the opcode, then rewind: the generated unpack_* helpers consume
    // the full 16-B record header + payload themselves
    uint16_t op = 0, rbytes = 0;
    r.take16(op);
    r.take16(rbytes);
    r.pos -= 4;
    switch (op) {
      case zhao_abi::ZHAO_OP_BEGIN_FRAME: {
        zhao_abi::ZhRecordBeginFrame c;
        zhao_unpack_begin_frame(r, c);
        tick = c.payload.frame_id;
        rr.frame_id = c.payload.frame_id;
        break;
      }
      case zhao_abi::ZHAO_OP_END_FRAME: {
        zhao_abi::ZhRecordEndFrame c;
        zhao_unpack_end_frame(r, c);
        // expected_framebuffer_crc is the EMIT-side expectation; the
        // renderer reports its own CRCs (Result) and W3.6 compares.
        break;
      }
      case zhao_abi::ZHAO_OP_NOP: {
        zhao_abi::ZhRecordNop c;
        zhao_unpack_nop(r, c);
        break;
      }
      case zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT: {
        zhao_abi::ZhRecordSetPresentationContract c;
        zhao_unpack_set_presentation_contract(r, c);
        mode_latched_ = c.payload.mode;  // effective NEXT frame start (§1.1)
        break;
      }
      case zhao_abi::ZHAO_OP_SET_VIEW: {
        zhao_abi::ZhRecordSetView c;
        zhao_unpack_set_view(r, c);
        if (c.payload.view_id < 2) {
          views[c.payload.view_id].active = true;
          for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b)
              views[c.payload.view_id].vp.m[a][b] =
                  fx16{(&c.payload.view_projection.m00)[a * 4 + b]};
          views[c.payload.view_id].pixel_error = c.payload.pixel_error;
        }
        break;
      }
      case zhao_abi::ZHAO_OP_TERRAIN_FIELD: {
        zhao_abi::ZhRecordTerrainField c;
        zhao_unpack_terrain_field(r, c);
        FieldApp app;
        app.prog = res.field_program(c.payload.program);
        app.cmd = c.payload;
        if (app.prog == nullptr) ++rr.resource_misses;
        fields.push_back(app);
        break;
      }
      case zhao_abi::ZHAO_OP_SURFACE_STAMP: {
        zhao_abi::ZhRecordSurfaceStamp c;
        zhao_unpack_surface_stamp(r, c);
        // the brush handle is fully described inline at Phase 3 (circle/
        // ring by radius/ring_width); the patch handle names the sheet
        const TerrainPatch* patch = res.terrain_patch(c.payload.patch);
        if (patch == nullptr) {
          ++rr.resource_misses;
        } else {
          stamp_surface(sheet_for(c.payload.patch), *patch, c.payload);
        }
        break;
      }
      case zhao_abi::ZHAO_OP_DRAW_PROCEDURAL: {
        zhao_abi::ZhRecordDrawProcedural c;
        zhao_unpack_draw_procedural(r, c);
        if (c.payload.kind != zhao_abi::FORGE_HEIGHTFIELD_PATCH) {
          ++rr.resource_misses;  // the one implemented forge kind (zidl)
          break;
        }
        const TerrainPatch* patch = res.terrain_patch(c.payload.program);
        const Material* mat = res.material(c.payload.material);
        if (patch == nullptr || mat == nullptr) {
          rr.resource_misses += (patch == nullptr) + (mat == nullptr);
          break;
        }
        terrain.push_back(TerrainInst{patch, c.payload.program, c.payload.transform, mat});
        break;
      }
      case zhao_abi::ZHAO_OP_DRAW_FORM: {
        zhao_abi::ZhRecordDrawForm c;
        zhao_unpack_draw_form(r, c);
        const FormPattern* form = res.form(c.payload.form);
        const FormTransform* xf = res.transform(c.payload.transform);
        if (form == nullptr || xf == nullptr) {
          rr.resource_misses += (form == nullptr) + (xf == nullptr);
          break;
        }
        forms.push_back(FormDraw{form, xf, c.payload.viewport_mask, c.payload.flags});
        break;
      }
      case zhao_abi::ZHAO_OP_DRAW_POPULATION: {
        zhao_abi::ZhRecordDrawPopulation c;
        zhao_unpack_draw_population(r, c);
        const Population* pop = res.population(c.payload.population);
        if (pop == nullptr) {
          ++rr.resource_misses;
          break;
        }
        pops.push_back(PopDraw{pop, c.payload.viewport_mask, c.payload.flags});
        break;
      }
      case zhao_abi::ZHAO_OP_DRAW_SKY: {
        zhao_abi::ZhRecordDrawSky c;
        zhao_unpack_draw_sky(r, c);
        const sky::SkySet* set = res.sky_set(c.payload.sky_set);
        if (set == nullptr) {
          ++rr.resource_misses;
          break;
        }
        has_sky = true;
        sky = SkyDraw{set, c};
        break;
      }
      case zhao_abi::ZHAO_OP_EMIT_AUDIO_EVENT: {
        zhao_abi::ZhRecordEmitAudioEvent c;
        zhao_unpack_emit_audio_event(r, c);
        rr.audio_events.push_back(AudioEventTrigger{c.payload.event_id, c.payload.pan_fx,
                                                    c.payload.gain, c.payload.sample_handle,
                                                    c.payload.timestamp});
        break;
      }
      default: {
        // debug umbrella / anything else: never game-facing, skip the record
        r.pos += rbytes;  // r.pos is at the record start (rewound above)
        break;
      }
    }
    ++rr.commands_executed;
  }

  // ---- execute the charter §8 passes --------------------------------------
  Viewport vps[2];
  const uint32_t nv = viewports_of(mode, vps);
  WorkSurface surf;
  sky::SkyColor bg{0, 0, 0};  // fallback when no DrawSky/set (sky §1)
  if (has_sky) bg = sky.set->background;
  surf.reset(canvas_width(mode), canvas_height(mode), bg);

  // emit once, render per view (emission is view-independent, §6)
  std::vector<sky::SkyPrimitive> prims;
  if (has_sky) {
    prims =
        sky::emit_layers(*sky.set, tick, angle16{sky.cmd.payload.drum_yaw}, sky.cmd.payload.flags);
  }

  const auto lerp8 = [](uint8_t a, uint8_t b, int num, int den) {
    return static_cast<uint8_t>(
        (static_cast<int32_t>(a) * (den - num) + static_cast<int32_t>(b) * num + den / 2) / den);
  };

  bool velocity_recorded = false;  // once per frame, first view (W3.5 note)
  for (uint32_t view = 0; view < nv; ++view) {
    if (!views[view].active) continue;  // no SetView for this view: skip
    const Viewport& vpp = vps[view];
    const mat4fx& vp = views[view].vp;

    // pass 1 + 6: sky (viewport_mask gates the view; rotation-only law §1)
    if (has_sky && (sky.cmd.payload.viewport_mask & (1u << view))) {
      const zhao_abi::ZhMat4fx& rm = sky.cmd.payload.rot_proj[view];
      mat4fx rp;
      for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b) rp.m[a][b] = fx16{(&rm.m00)[a * 4 + b]};
      if (sky::rot_proj_is_rotation_only(rp)) {
        // sky_and_beams §1 pins the pass-6 sub-order as SUN BEFORE CLOUD, but
        // emit_layers emits the cloud sheet first (it is the §1.1 row above
        // the sun). Rastering in emission order therefore violated the pinned
        // order and put the cloud under the sun's additive contribution.
        // Sweep the primitive list twice: everything except Cloud (bands,
        // cap, under, sun — already in pass order), then Cloud.
        for (int sweep = 0; sweep < 2; ++sweep) {
        const bool cloud_sweep = sweep == 1;
        for (const sky::SkyPrimitive& p : prims) {
          if ((p.layer == sky::SkyLayer::Cloud) != cloud_sweep) continue;
          ScreenV sv[3];
          bool ok = true;
          for (int k = 0; k < 3; ++k) {
            const ProjOut o = project_vertex(rp, vpp, p.v[k].x, p.v[k].y, p.v[k].z, &rr.sat);
            if (!o.in) {
              ok = false;
              break;
            }
            sv[k] = o.s;
            sv[k].a = static_cast<int32_t>(p.v[k].alpha) << 8;  // Q16.16
          }
          if (!ok) continue;
          TriMode m;
          uint8_t cr = 0, cg = 0, cb = 0;
          switch (p.layer) {
            case sky::SkyLayer::BandLower:
              cr = lerp8(sky.set->band_lower_horizon.r, sky.set->band_lower_top.r, p.row,
                         sky::kBandRows);
              cg = lerp8(sky.set->band_lower_horizon.g, sky.set->band_lower_top.g, p.row,
                         sky::kBandRows);
              cb = lerp8(sky.set->band_lower_horizon.b, sky.set->band_lower_top.b, p.row,
                         sky::kBandRows);
              m.depth_test = false;
              m.depth_write = true;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkyFarDepth;
              break;
            case sky::SkyLayer::BandUpper:
              cr = lerp8(sky.set->band_upper_bottom.r, sky.set->band_upper_top.r, p.row,
                         sky::kBandRows);
              cg = lerp8(sky.set->band_upper_bottom.g, sky.set->band_upper_top.g, p.row,
                         sky::kBandRows);
              cb = lerp8(sky.set->band_upper_bottom.b, sky.set->band_upper_top.b, p.row,
                         sky::kBandRows);
              m.depth_test = false;
              m.depth_write = true;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkyFarDepth;
              break;
            case sky::SkyLayer::Cap:
              cr = sky.set->cap.r;
              cg = sky.set->cap.g;
              cb = sky.set->cap.b;
              m.depth_test = false;
              m.depth_write = true;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkyFarDepth;
              break;
            case sky::SkyLayer::Under:
              cr = sky.set->under.r;
              cg = sky.set->under.g;
              cb = sky.set->under.b;
              m.depth_test = true;
              m.depth_write = true;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkyUnderDepth;
              break;
            case sky::SkyLayer::Cloud:
              cr = sky.set->cloud.r;
              cg = sky.set->cloud.g;
              cb = sky.set->cloud.b;
              m.depth_test = true;
              m.depth_write = false;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkyCloudDepth;
              m.blend = BlendMode::kAlpha;
              m.interp_alpha = true;
              break;
            case sky::SkyLayer::Sun:
              cr = sky.set->sun.r;
              cg = sky.set->sun.g;
              cb = sky.set->sun.b;
              m.depth_test = true;
              m.depth_write = false;
              m.use_fixed_depth = true;
              m.fixed_depth = kSkySunDepth;
              m.blend = BlendMode::kAdditive;
              m.interp_alpha = true;
              break;
          }
          raster_tri(surf, vpp, sv[0], sv[1], sv[2], cr, cg, cb, m);
        }
        }
      }
    }

    // pass 3a: heightfield patches (TerrainField applied inside, once per
    // view; the velocity lane recorded on the first view only)
    for (const TerrainInst& t : terrain) {
      const SurfaceSheet* sheet = nullptr;
      for (const auto& e : sheets_)
        if (e.first == t.patch_handle) sheet = &e.second;
      draw_heightfield(surf, vpp, vp, *t.patch, t.xform, *t.mat, sheet, fields, tick,
                       velocity_recorded ? nullptr : &rr.terrain_velocity, &rr.sat);
    }
    velocity_recorded = true;

    // pass 3b: DrawForm markers (opaque, depth test + write)
    for (const FormDraw& f : forms) {
      if (f.viewport_mask & (1u << view))
        draw_form_marker(surf, vpp, vp, *f.form, *f.xf, f.flags, &rr.sat);
    }

    // pass 7: DrawPopulation sprites (depth test only)
    for (const PopDraw& p : pops) {
      if (p.viewport_mask & (1u << view)) draw_population(surf, vpp, vp, *p.pop, p.flags, &rr.sat);
    }
  }

  // pass 9: resolve (charter §8: ordered dither on the RGB565 resolve) + CRC
  uint8_t* out = canvas.slot[dst_slot].data();
  resolve_rgb565(surf.rgb.data(), surf.w, surf.h, out);
  rr.canvas_crc32c = canvas_crc32c(mode, out);
  rr.displayed_crc32c = displayed_crc32c(mode, out);
  return rr;
}

bool tone_id_for(uint32_t sample_index, ToneId* out) {
  // audio_rules.md §4 frozen tone table (the only legal Phase-2 tones)
  switch (sample_index) {
    case 0:
      *out = ToneId::TONE_A4;
      return true;
    case 1:
      *out = ToneId::TONE_A5;
      return true;
    case 2:
      *out = ToneId::TONE_C4;
      return true;
  }
  return false;
}

}  // namespace render
}  // namespace zref
