# ZRef — the C++17 golden-reference library (charter 20).
# W3: fixed-point core is header-only constexpr (zref_fixp/rcp/trig/sat +
#     generated tables). W4 adds the frame packet + .zcap container
#     (zref_frame) and sha256; W6 adds the shell loop here.

add_library(zhao_zref STATIC
  src/zref_frame.cpp
  src/zfield/zfield_decode.cpp
  src/zfield/zfield_interpret.cpp)
target_include_directories(zhao_zref PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/runtime/include)  # generated zhao_abi.h
target_compile_features(zhao_zref PUBLIC cxx_std_17)
