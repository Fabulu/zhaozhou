# Desktop runtime product (charter §3.5). Phase-1 scope is the documented
# EMPTY stub main (charter §23 Phase-1 build list; plan W6): it links ZRef
# and drives the empty-frame shell as its frame loop. The MiSTer target
# (runtime/mister) stays hardware-blocked (ZH-000, sys/ vendoring).
add_executable(zhao-desktop desktop_main.cpp)
target_link_libraries(zhao-desktop PRIVATE zhao_zref)
target_compile_features(zhao-desktop PRIVATE cxx_std_17)
