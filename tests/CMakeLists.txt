# Tests. CTest labels (charter 20.3, P1 findings 4):
#   fast    — every commit (cmake/ctest default gate)
#   nightly — full fuzz + capture replay
#   formal  — SymbiYosys; wrapper tests SKIP when the suite is absent
#   lint    — per-module verilator --lint-only -Wall
# The Verilator harness + stub-top tests land in the next W1 commit.
