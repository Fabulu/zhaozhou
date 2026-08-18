# ZRef — the C++17 golden-reference library (charter 20).
# W3: fixed-point core is header-only constexpr (zref_fixp/rcp/trig/sat +
#     generated tables). W4 adds the frame packet + .zcap container
#     (zref_frame) and sha256; W6 adds the shell loop (zref.cpp).
# W2.2 adds the video subsystem (zref_video: mode/framectl/scanout/scaler
#     mirrors; the displayed-stream CRC lives in zrender/resolve.cpp).
# W2.4 adds the audio subsystem (MixerTone tone subset + AudioFifo oracle).
# W3.5 adds the software renderer (zrender/) + the sky emitter (zsky/).
# Phase 4 adds zref::EdgeWalk (zrender/edgewalk.cpp) — the RASTER.EDGEWALK
#     tile-coverage oracle, a view onto rast.cpp's frozen fill law — plus
#     zref::TileStore (zrender/tilestore.cpp, the RASTER.TILESTORE ping-pong
#     model) and zref::TileResolve (zrender/tileresolve.cpp, a view onto
#     resolve.cpp's frozen ordered dither).
# Phase 4/5 raster completion adds zref::FragmentPipeline (zrender/
#     fragment.cpp — RASTER.FRAGMENT's depth/stencil/blend, calling the frozen
#     zref::unit_mul rather than restating it) and zref::EarlyZ (zrender/
#     earlyz.cpp — RASTER.EARLYZ's conservative hierarchical-Z floor and the
#     coarse transparent-depth bins).
# World-identity wave adds the creature/character reference core
#     (zcreature/: ring meshlets, pose bank, skinning, alive laws, LOD).

add_library(zhao_zref STATIC
  src/zref_frame.cpp
  src/zref.cpp
  src/zref_audio.cpp
  src/zref_video.cpp
  src/zfield/zfield_decode.cpp
  src/zfield/zfield_interpret.cpp
  src/zterrain/terrain_core.cpp
  src/zrender/render_frame.cpp
  src/zrender/rast.cpp
  src/zrender/edgewalk.cpp
  src/zrender/terrain.cpp
  src/zrender/sprites.cpp
  src/zrender/resolve.cpp
  src/zrender/tilestore.cpp
  src/zrender/tileresolve.cpp
  src/zrender/earlyz.cpp
  src/zrender/fragment.cpp
  src/zsky/emit_layers.cpp
  src/zsky/star_gamut.cpp
  src/zsky/star_bake.cpp
  src/zsky/star_flare.cpp
  src/zsky/star_field.cpp
  src/zsky/star_compose.cpp
  src/zsky/env_state.cpp
  src/zcreature/creature_core.cpp
  src/zcreature/creature_sim.cpp)
target_include_directories(zhao_zref PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/runtime/include)  # generated zhao_abi.h
target_compile_features(zhao_zref PUBLIC cxx_std_17)
