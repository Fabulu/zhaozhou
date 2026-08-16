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
  ${CMAKE_SOURCE_DIR}/compiler/tests/generated)
