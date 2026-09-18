ZHAOZHOU TRUE-CONSOLE COMPLETION HANDOFF
======================================

Start with Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt.
It includes the architecture, 38-capability checklist, implementation packets,
acceptance/abort rules, sources and full supporting source appendix.

No repository was modified. Pinned refresh: 155c141f5b922a62b9331c048352e46c3f67ad04.
Review newer/local work before implementation; do not duplicate concurrent work.

Implemented and executed here:
  python -m unittest discover -s tests -v
  25 passing tests (test output in evidence/python-tests.txt).

Implemented but NOT HDL-simulated or synthesized here:
  implementation/zhao_cpl_ram_fifo.sv
  implementation/zhao_cpl_upload_guard.sv
  tests/rtl_candidate_tb.sv
  python tests/run_rtl_tests.py
The runner exits 2 when Icarus tools are absent; that is not a pass.

The support models are NOT complete particle-force/FIELD/lighting oracles.
The strict-prefix spawn admission policy and external stable-ID adapter are
explicit proposals requiring reconciliation before production adoption.

examples/completion_seed.json deliberately FAILS --release until all obligations
and target fit/timing/workload/board evidence exist. It is not an auto-generated
proof of the repository's current scope. The checker cannot prove that a claimed
source/test/receipt is semantically correct: integration tests and review must.

No FPGA ALM/DSP/M10K saving or timing number is claimed for the supplied code.
