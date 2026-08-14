# ZRef — the C++17 golden-reference library (charter 20).
# W4 lands the frame packet + .zcap container (zref_frame) and sha256; W3
# adds zref_fixp/trig/rcp in parallel; W6 adds the shell loop.

add_library(zhao_zref STATIC
  src/zref_frame.cpp)
target_include_directories(zhao_zref PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/runtime/include)  # generated zhao_abi.h
