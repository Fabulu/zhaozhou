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
# TWO REPORTS, AND THE SECOND ONE EXISTS BECAUSE OF A SIZE LIMIT.
#
# MEASURED 2026-09-03: tools/quartus/internal_paths.py found that for
# zhao_texture_aux_pipe, zhao_texture_rsp_dispatch and one seed of
# zhao_raster_rcp24_svc, ALL 200 worst paths touched a top-level port -- so the
# report contained no register-to-register path at all and could say nothing
# about the block's own logic. A leaf with hundreds of virtual pins fills its
# worst-path list with its boundary before reaching its arithmetic.
#
# Raising the FULL-DETAIL report to 2000 paths fixed the blindness and made the
# archive uncommittable: 1.7 MB became 10-17 MB a block, and the repository's
# pre-commit hook refused it, correctly. Large files are almost never evidence.
#
# So: 200 paths at full detail, which is what diagnosing a cone needs and what
# the 2026-08-25 census directive asked for, plus 2000 paths at SUMMARY detail,
# which is one line each and is all the internal/boundary split needs. The
# summary report is about 300 KB.
report_timing -setup -npaths 200 -nworst 1 -detail full_path \
    -file output_files/blockfit_setup_paths.rpt
report_timing -hold  -npaths 200 -nworst 1 -detail full_path \
    -file output_files/blockfit_hold_paths.rpt
report_timing -setup -npaths 2000 -nworst 1 -detail summary \
    -file output_files/blockfit_setup_summary.rpt

# THE MARGIN BAND, AND WHY A FIXED ROW COUNT CANNOT PRODUCE IT.
#
# MEASURED 2026-09-16 against the retained Timing3 summary: the BEST slack in
# all 2000 exported rows is +0.582 ns. A comfortable 110 MHz target needs every
# path below +0.909 ns, so the entire export sits inside the band and the true
# population is unknown -- the table stops before the margin does. The census
# tool now says so (`truncated_by_export`) instead of reporting a tidy count.
#
# That could not be repaired after the fact. The Timing3 work directory and its
# TimeQuest database were gone by the time the question was asked, so the only
# way to answer it was another fit. Hence this third report: it is bounded by
# SLACK rather than by a row count, so it covers the band by construction
# however many paths turn out to be in it.
#
# 1.35 ns spans the 115 MHz planning view as well, so one export serves both.
# -nworst 1 keeps one path per endpoint, so the row count is bounded by endpoint
# count rather than by bus width, and summary detail is one line per row.
report_timing -setup -npaths 100000 -nworst 1 -less_than_slack 1.35 \
    -detail summary -file output_files/blockfit_setup_margin.rpt
delete_timing_netlist
project_close
