// env_state.cpp — the §4a environment-state record (sky_and_beams v1.2):
// serialize/deserialize for the ENVIRONMENT_STATE .zcap chunk (0x000C).
//
// Law (in citation order):
//   spec/sky_and_beams.md   §4a  the record's semantics (sun direction,
//                                vertex light, global tint, fog pointer,
//                                defaults) and the chunk law
//   spec/capture_format.md  §4.2 [v3] ENVIRONMENT_STATE body: 20 B fixed
//                                little-endian, byte-mirror of the
//                                SetEnvironment payload
//   spec/commands.zidl      SetEnvironment 0x0311 (reserved since ABI v3)
//   spec/qformats.md        §2   angle16 / fx16 / unit8 / colour8-565
//
// Integer-only (charter §29-7); the serializer mirrors the celestial_state
// style (star_gamut.cpp) — plain LE puts/takes, no bitfield machinery.

#include "zref/zref_sky.hpp"

namespace zref {
namespace sky {

void env_state_serialize(const EnvState& st, uint8_t out[kEnvStateBytes]) {
  uint8_t* p = out;
  const auto put16 = [&p](uint16_t v) {
    *p++ = static_cast<uint8_t>(v);
    *p++ = static_cast<uint8_t>(v >> 8);
  };
  const auto put32 = [&p](uint32_t v) {
    for (int i = 0; i < 4; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
  };
  put16(st.sun_yaw.raw);
  put16(st.sun_pitch.raw);
  put16(st.sun_colour.bits);
  put16(st.ambient.bits);
  put16(st.tint.bits);
  *p++ = st.tint_strength;
  *p++ = static_cast<uint8_t>(st.fog);
  put32(static_cast<uint32_t>(st.fog_near.raw));
  put32(static_cast<uint32_t>(st.fog_far.raw));
}

EnvState env_state_deserialize(const uint8_t in[kEnvStateBytes]) {
  EnvState st;
  const uint8_t* p = in;
  const auto take16 = [&p]() -> uint16_t {
    const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    return v;
  };
  const auto take32 = [&p]() -> uint32_t {
    const uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                       (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return v;
  };
  st.sun_yaw = angle16{take16()};
  st.sun_pitch = angle16{take16()};
  st.sun_colour = rgb565{take16()};
  st.ambient = rgb565{take16()};
  st.tint = rgb565{take16()};
  st.tint_strength = *p++;
  // fog mode is a u8 enum on the wire (fog_mode, commands.zidl); values
  // beyond the member set cannot arrive from a validated record — clamp to
  // Off so a corrupt chunk still deserializes deterministically.
  const uint8_t fog_raw = *p++;
  st.fog = fog_raw <= static_cast<uint8_t>(FogMode::Linear) ? static_cast<FogMode>(fog_raw)
                                                            : FogMode::Off;
  st.fog_near = fx16{static_cast<int32_t>(take32())};
  st.fog_far = fx16{static_cast<int32_t>(take32())};
  return st;
}

}  // namespace sky
}  // namespace zref
