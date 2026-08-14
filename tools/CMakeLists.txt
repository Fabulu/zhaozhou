# Native (C++) tool helpers. TS/Node tools (abi-gen, fixgen, ledger, ...) are
# npm workspaces, not CMake.

# zhao-capture: .zcap CLI (SW.TOOLS.CAPTURE, spec/capture_format.md 4)
add_executable(zhao-capture capture/zhao_capture.cpp)
target_link_libraries(zhao-capture PRIVATE zhao_zref)
