# Run a positive control whose whole job is to die, and judge it by what it
# PRINTED rather than by how it died.
#
# THE PROBLEM. Verilator ends `$fatal`/`$stop` in an abort path that, on Windows
# with captured output, does not return. early_desc_layout_guard prints its
# expected text in 12 ms when run standalone with output to a file, and then
# sits at 0 CPU for its whole timeout when ctest gives it pipes. Closing stdin
# does not change it. For a long time that read as "the guard no longer fires",
# which is the worst available misreading: it fires perfectly, and a positive
# control believed dead is a control nobody trusts.
#
# THE RULE, and why it does not quietly weaken the gate. The claim under test is
# "this elaboration check HALTS the run with this message". The message is the
# evidence for that claim; the exit path is not. So:
#
#   * expected text present  -> PASS, however the process ended
#   * expected text absent   -> FAIL, however the process ended
#
# A guard that stopped firing prints nothing and is caught by the second branch,
# which is the failure this control exists to detect. A timeout is never by
# itself a pass -- it is only ever tolerated alongside the diagnostic that
# proves the guard already did its job.
#
# Required: -DEXE=<path> -DEXPECT=<regex>
# Optional: -DRUN_TIMEOUT=<seconds, default 60>

if(NOT DEFINED EXE OR NOT DEFINED EXPECT)
  message(FATAL_ERROR "run_expect_fatal: -DEXE and -DEXPECT are required")
endif()
if(NOT DEFINED RUN_TIMEOUT)
  set(RUN_TIMEOUT 60)
endif()

# A file that exists and is empty: the child reads EOF instead of blocking.
set(_empty "${CMAKE_CURRENT_BINARY_DIR}/run_expect_fatal.stdin")
file(WRITE "${_empty}" "")

execute_process(
  COMMAND "${EXE}"
  INPUT_FILE "${_empty}"
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE rc
  TIMEOUT ${RUN_TIMEOUT})

set(combined "${out}${err}")
message(STATUS "${combined}")

if(NOT combined MATCHES "${EXPECT}")
  message(FATAL_ERROR
    "run_expect_fatal: expected output matching\n  ${EXPECT}\n"
    "but the control printed:\n${combined}\n"
    "(process result: ${rc})\n"
    "A positive control that stops firing is the one to check hardest.")
endif()

if(rc EQUAL 0)
  message(FATAL_ERROR
    "run_expect_fatal: ${EXE} printed the expected text but exited 0. "
    "The guard must halt the simulation, not merely warn about it.")
endif()

if(rc STREQUAL "Process terminated due to timeout")
  message(STATUS
    "run_expect_fatal: control FIRED (the diagnostic above is the evidence); "
    "the process then failed to exit within ${RUN_TIMEOUT}s, which is the "
    "known Verilator abort-under-captured-output behaviour on Windows and not "
    "a statement about the design.")
else()
  message(STATUS "run_expect_fatal: control fired and exited (rc=${rc})")
endif()
