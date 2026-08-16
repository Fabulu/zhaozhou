# ZRef — the C++17 golden-reference library (charter 20).
# W3: fixed-point core is header-only constexpr (zref_fixp/rcp/trig/sat +
#     generated tables). W4 adds the frame packet + .zcap container
#     (zref_frame) and sha256; W6 adds the shell loop (zref.cpp).
# W2.2 adds the video subsystem (zref_video: mode/framectl/scanout/scaler
#     mirrors; the displayed-stream CRC lives in zrender/resolve.cpp).
# W2.4 adds the audio subsystem (MixerTone tone subset + AudioFifo oracle).
# W3.5 adds the software renderer (zrender/) + the sky emitter (zsky/).

add_library(zhao_zref STATIC
  src/zref_frame.cpp
  src/zref.cpp
  src/zref_audio.cpp
  src/zref_video.cpp
  src/zfield/zfield_decode.cpp
  src/zfield/zfield_interpret.cpp
  src/zrender/render_frame.cpp
  src/zrender/rast.cpp
  src/zrender/terrain.cpp
  src/zrender/sprites.cpp
  src/zrender/resolve.cpp
  src/zsky/emit_layers.cpp)
target_include_directories(zhao_zref PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/runtime/include)  # generated zhao_abi.h
target_compile_features(zhao_zref PUBLIC cxx_std_17)
