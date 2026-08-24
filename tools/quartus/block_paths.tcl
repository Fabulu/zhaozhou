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
report_timing -setup -npaths 25 -nworst 1 -detail full_path \
    -file output_files/blockfit_setup_paths.rpt
report_timing -hold  -npaths 25 -nworst 1 -detail full_path \
    -file output_files/blockfit_hold_paths.rpt
delete_timing_netlist
project_close
