# Core-specific constraints for the safe SuperStation bring-up image.
# The canonical root clocks, clock uncertainty, and framework exceptions are
# sourced through sys/sys.qip -> sys/sys_top.sdc. The core runs directly from
# FPGA_CLK2_50 and introduces no generated clock.
