# Native (C++) tool helpers. TS/Node tools (abi-gen, fixgen, ledger, ...) are
# npm workspaces, not CMake.

# zhao-capture: .zcap CLI (SW.TOOLS.CAPTURE, spec/capture_format.md 4)
add_executable(zhao-capture capture/zhao_capture.cpp)
target_link_libraries(zhao-capture PRIVATE zhao_zref)

# zhao-reel: deterministic frame-sequence capture for the deformation reel
# (drives zref::render across successive ticks; enforces the <=256-colour
# palette-exact law the site's GIF encode requires). Includes the tests/render
# packet helpers and the TS-generated field-program wrappers read-only.
add_executable(zhao-reel reel/zhao_reel.cpp)
target_link_libraries(zhao-reel PRIVATE zhao_zref)
target_include_directories(zhao-reel PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated
  ${CMAKE_SOURCE_DIR}/reference/src)  # white-box: zrender/internal.hpp (creature sim lattice)

# Direction 14: public visible-skin proof for all five Manafold carriers.
add_executable(manafold-public-jointgate reel/manafold_public_jointgate.cpp)
target_link_libraries(manafold-public-jointgate PRIVATE zhao_zref)
target_include_directories(manafold-public-jointgate PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated
  ${CMAKE_SOURCE_DIR}/reference/src)

# Pass 17: Manafold identity-default L/R eye-size and form-control gate.
add_executable(manafold-eyesize reel/manafold_eyesize.cpp)
target_link_libraries(manafold-eyesize PRIVATE zhao_zref)
target_include_directories(manafold-eyesize PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/render
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated
  ${CMAKE_SOURCE_DIR}/reference/src)
