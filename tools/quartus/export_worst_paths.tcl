project_open zhao_shell_fit
create_timing_netlist
read_sdc
update_timing_netlist
set fh [open "output_files/worst_paths400.txt" w]
puts $fh "Worst setup paths, Slow 1100mV 100C, gpu_clk"
set paths [get_timing_paths -setup -npaths 400 -detail summary -to_clock gpu_clk]
foreach_in_collection p $paths {
    set slack [get_path_info $p -slack]
    set from  [get_node_info [get_path_info $p -from] -name]
    set to    [get_node_info [get_path_info $p -to] -name]
    puts $fh [format "%8.3f  %s  ->  %s" $slack $from $to]
}
close $fh
delete_timing_netlist
project_close
