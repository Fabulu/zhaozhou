# Genuine post-map boundary witness. quartus_sta loads the mapped netlist with
# --post_map before sourcing this file; source declarations alone cannot create
# any row below.
set rows {}
foreach_in_collection port [get_ports *] {
    set name [get_port_info -name $port]
    if {[get_port_info -is_input_port $port]} {
        set direction input
        set mapped_count [get_collection_size [get_fanouts [list $name]]]
    } elseif {[get_port_info -is_output_port $port]} {
        set direction output
        set mapped_count [get_collection_size [get_fanins [list $name]]]
    } else {
        error "Unsupported mapped top-level port direction for $name"
    }
    lappend rows [list port $name $direction $mapped_count]
}

# Query the complete netlist before creating the result so an API failure cannot
# leave a partial witness that looks like a complete table.
set out_dir [file join [get_global_assignment -name PROJECT_OUTPUT_DIRECTORY] characterization]
file mkdir $out_dir
set witness [open [file join $out_dir post_map_connectivity.tsv] w]
puts $witness "record\tname\tdirection\tmapped_endpoint_count"
foreach row [lsort -dictionary -index 1 $rows] {
    puts $witness [join $row "\t"]
}
close $witness
post_message -type info "Wrote post-map shell boundary witness to $out_dir/post_map_connectivity.tsv"
