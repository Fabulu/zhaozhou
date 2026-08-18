# Custom TimeQuest report script. quartus_sta loads the fitted netlist and SDC
# before sourcing this file via --report_script. Keep extraction text-based and
# deterministic so the PowerShell runner can normalize it into committed JSON.

set out_dir [file join [get_global_assignment -name PROJECT_OUTPUT_DIRECTORY] characterization]
file mkdir $out_dir

proc write_analysis_metric {fh analysis max_paths} {
    set paths [get_timing_paths -$analysis -npaths 1 -nworst 1]
    if {[get_collection_size $paths] == 0} {
        puts $fh "analysis\t$analysis\tNA\t0"
        return
    }

    set worst "NA"
    foreach_in_collection path $paths {
        set worst [get_path_info -slack $path]
    }

    # With -nworst 1 this is a failing-endpoint count, not an attempted count
    # of every combinatorial path through an endpoint.
    set failing [get_timing_paths -$analysis -npaths $max_paths -nworst 1 -less_than_slack 0]
    puts $fh "analysis\t$analysis\t$worst\t[get_collection_size $failing]"
}

set metric_file [open [file join $out_dir timing_metrics.tsv] w]
puts $metric_file "record\tname\tvalue\tcount"
foreach_in_collection clock [get_clocks *] {
    puts $metric_file "clock\t[get_clock_info -name $clock]\t[get_clock_info -period $clock]\t0"
}

set max_paths [expr {[get_collection_size [all_registers]] + [get_collection_size [all_outputs]] + 1}]
foreach analysis {setup hold recovery removal} {
    write_analysis_metric $metric_file $analysis $max_paths
}
close $metric_file

report_clocks -file [file join $out_dir clocks.rpt]
report_clock_transfers -file [file join $out_dir clock_transfers.rpt]
report_ucp -summary -file [file join $out_dir unconstrained_paths.rpt]

foreach analysis {setup hold recovery removal} {
    report_timing -$analysis -npaths 100 -nworst 1 -detail full_path \
        -file [file join $out_dir ${analysis}_paths.rpt]
}

post_message -type info "Wrote deterministic shell characterization reports to $out_dir"
