# ZRef — the C++17 golden-reference library (charter 20).
# W3: fixed-point core is header-only constexpr (zref_fixp/rcp/trig/sat +
#     generated tables); later waves add frame, zfield and the shell here.
add_library(zhao_zref INTERFACE)
target_include_directories(zhao_zref INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(zhao_zref INTERFACE cxx_std_17)
