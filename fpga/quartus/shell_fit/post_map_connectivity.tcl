# Genuine post-map boundary witness. quartus_sta loads the mapped netlist with
# --post_map before sourcing this file; source declarations alone cannot create
# any row below.
set out_dir [file join [get_global_assignment -name PROJECT_OUTPUT_DIRECTORY] characterization]
file mkdir $out_dir
set witness [open [file join $out_dir post_map_connectivity.tsv] w]
puts $witness "record\tname\tdirection\tpost_map_count"
puts $witness "top\tzhao_shell_fit_top\t-\t1"

set expected_ports {
    {gpu_clk input}
    {vid_clk input}
    {audio_clk input}
    {rst_n input}
    {{fit_signature_o[0]} output}
    {{fit_signature_o[1]} output}
    {{fit_signature_o[2]} output}
    {{fit_epoch_o[0]} output}
    {{fit_epoch_o[1]} output}
    {{fit_epoch_o[2]} output}
}
foreach spec $expected_ports {
    lassign $spec name direction
    set matches [get_ports -nowarn [list $name]]
    puts $witness "port\t$name\t$direction\t[get_collection_size $matches]"
}
close $witness
post_message -type info "Wrote post-map shell boundary witness to $out_dir/post_map_connectivity.tsv"
