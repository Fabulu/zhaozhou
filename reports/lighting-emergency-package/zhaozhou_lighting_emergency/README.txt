ZHAOZHOU LIGHTING EMERGENCY PACKAGE — 18 September 2026

Read LIGHTING_RESCUE_HANDOFF.txt first.

EXECUTED HERE:
  python test_light_model.py
  Nine software test methods pass. Detailed counts are in verification_results.json.
  Arithmetic is compared against independent Python integer / isqrt operations.
  Cycle models test stalls, bubbles, reset abandonment and order. They are NOT HDL
  simulation. The 480k-term calendar is a slot model, not connected renderer timing.

NOT EXECUTED HERE:
  RTL lint, RTL simulation, repository C++ oracle differential, Quartus map/fit,
  complete GEOM.LIGHT replacement, connected-console or board run.

CANDIDATE RTL:
  rtl/zhao_light_div32_ii2.sv — exact signed saturating quotient, II2 architecture.
  rtl/zhao_light_isqrt64_ii8.sv — exact floor square root, II8 architecture.

Suggested first HDL smoke (from this directory; on the actual dev machine):
  verilator --binary --timing --assert -Wall --top-module tb_lightarith \
    rtl/zhao_light_div32_ii2.sv rtl/zhao_light_isqrt64_ii8.sv tests/tb_lightarith.sv
  ./obj_dir/Vtb_lightarith

That command is not a successful-run claim. Resolve warnings and compatibility
problems, then run the existing project lint/Quartus-17 subset checks. Do not
silence new warnings to make a candidate appear validated. Add reset-in-flight,
random input bubbles and assertions to the HDL tests before adoption: the supplied
HDL smoke covers directed/random arithmetic and output stalls, not all those cases.

The running agent should rebase the proposal onto its current source before using
it. No repository files or active worktrees were changed by this delivery.
