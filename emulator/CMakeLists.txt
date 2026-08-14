# ZEmu — the desktop emulator product (charter §3.3/§20.2). Phase-1 scope is
# the documented-EMPTY stub main (charter §23 Phase-1 build list: "empty
# ZEmu"; plan W6): it links ZRef and replays sealed frames through the
# empty-frame shell. The real console simulator lands in later phases.
add_executable(zemu zemu_main.cpp)
target_link_libraries(zemu PRIVATE zhao_zref)
target_compile_features(zemu PRIVATE cxx_std_17)
