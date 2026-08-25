# block_paths.tcl — node-level timing paths for the per-block / per-pair fit lane.
#
# WHY: run_block_fit.ps1 read an Fmax out of blockfit.sta.rpt and stopped there.
# That is enough to RANK blocks and useless for DIAGNOSING one. The pair campaign
# of 2026-08-24 ranked four renderer seams at 31.10 / 37.25 / 55.52 / 88.79 MHz
# and then could not answer the first question the ranking raised -- which side
# of the slowest pair is slow -- because no node-level report was ever produced.
#
# The shell lane has had report.tcl doing exactly this since it was written. The
# block lane simply never got it.
project_open blockfit
create_timing_netlist -model slow
read_sdc
update_timing_netlist
# 25 PATHS SHOWED THE WINNER AND NOTHING ELSE, and wave 8 proved that is not
# enough to plan with: removing the #1 cone bought 2% because a second cone of
# equal length stood behind it, and the 25-path report never showed it. Per the
# owner directive of 2026-08-25 the census covers the top 200 setup paths so
# distinct cone FAMILIES can be counted.
#
# `-nworst 1` keeps one path per ENDPOINT, so 200 rows are 200 different
# endpoints rather than 200 bits of one bus -- which is what makes the grouping
# meaningful. full_path detail carries the module hierarchy the census groups on.
report_timing -setup -npaths 200 -nworst 1 -detail full_path \
    -file output_files/blockfit_setup_paths.rpt
report_timing -hold  -npaths 200 -nworst 1 -detail full_path \
    -file output_files/blockfit_hold_paths.rpt
delete_timing_netlist
project_close
