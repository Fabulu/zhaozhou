# Core-specific constraints for the safe SuperStation bring-up image.
# Root clocks, generated PLL clocks, clock uncertainty, and framework exceptions
# are sourced through sys/sys.qip -> sys/sys_top.sdc and rtl/pll.qip. The core
# uses the pinned Template_MiSTer 50 MHz -> 20 MHz PLL without another clock.
