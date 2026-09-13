# capture_dual18_atom_routes.tcl -- genuine Quartus post-map atom graph capture.
#
# Run only under the canonical quartus_cdb executable after quartus_map.  This
# script reads the compiler database with -type map; it never elaborates source
# and never writes a modified database.  The TSV records only actual CDB
# node/port fanin/fanout adjacency for an independent Python checker.  It does
# not invent internal data arcs through atoms.  A generated .vo is optional
# audit material, not route evidence.

package require ::quartus::project 2.0
package require ::quartus::atoms 1.0

proc fail {message} {
    post_message -type error $message
    error $message
}

proc tsv_escape {value} {
    return [string map [list "\\" "\\\\" "\t" "\\t" "\r" "\\r" "\n" "\\n"] $value]
}

proc emit {channel fields} {
    set escaped {}
    foreach field $fields {
        lappend escaped [tsv_escape $field]
    }
    puts $channel [join $escaped "\t"]
}

proc endpoint_pairs {value} {
    if {$value eq "" || $value eq "-1"} { return {} }
    set nested 1
    foreach item $value {
        if {[llength $item] != 2} { set nested 0; break }
    }
    if {$nested} { return $value }
    if {[expr {[llength $value] % 2}] != 0} {
        error "endpoint list has odd cardinality: $value"
    }
    set result {}
    for {set i 0} {$i < [llength $value]} {incr i 2} {
        lappend result [list [lindex $value $i] [lindex $value [expr {$i + 1}]]]
    }
    return $result
}

proc atom_info_or_error {channel node key scope had_error_name} {
    upvar 1 $had_error_name had_error
    if {[catch {get_atom_node_info -node $node -key $key} value]} {
        emit $channel [list error $scope $node $key $value]
        set had_error 1
        return ""
    }
    return $value
}

proc port_info_or_error {channel node direction port key scope had_error_name} {
    upvar 1 $had_error_name had_error
    if {[catch {get_atom_port_info -node $node -type $direction -port_id $port -key $key} value]} {
        emit $channel [list error $scope $node $direction $port $key $value]
        set had_error 1
        return ""
    }
    return $value
}

if {[llength $argv] != 5} {
    fail "usage: capture_dual18_atom_routes.tcl <project> <revision> <atom.tsv> <atom.vo> <capture-id>"
}

set project_arg [lindex $argv 0]
set revision [lindex $argv 1]
set output_tsv [lindex $argv 2]
set output_vo [lindex $argv 3]
set capture_id [lindex $argv 4]

if {[file pathtype $project_arg] ne "absolute" ||
    [file pathtype $output_tsv] ne "absolute" ||
    [file pathtype $output_vo] ne "absolute"} {
    fail "project and output paths must be absolute"
}
if {![regexp {^[0-9a-f]{64}$} $capture_id]} {
    fail "capture-id must be a 64-digit lowercase SHA-256"
}
if {[file exists $output_tsv] || [file exists $output_vo]} {
    fail "refusing pre-existing atom-route output"
}
if {![file isdirectory [file dirname $output_tsv]] ||
    ![file isdirectory [file dirname $output_vo]]} {
    fail "atom-route output directories must already exist"
}

set tmp_tsv "${output_tsv}.partial"
if {[file exists $tmp_tsv]} {
    fail "refusing pre-existing partial atom-route output"
}

set channel [open $tmp_tsv {WRONLY CREAT EXCL}]
fconfigure $channel -encoding utf-8 -translation lf
set had_error 0
set project_opened 0
set netlist_loaded 0
set vo_status "not-requested"
set vo_error ""

if {[catch {
    project_open -error_on_incompatible_database -revision $revision $project_arg
    set project_opened 1
    read_atom_netlist -type map
    set netlist_loaded 1

    emit $channel [list schema dual18-atom-route-tsv 2]
    emit $channel [list meta artifact_class genuine-quartus-cdb-post-map]
    emit $channel [list meta synthetic false]
    emit $channel [list meta capture_id $capture_id]
    emit $channel [list meta project [file normalize $project_arg]]
    emit $channel [list meta revision $revision]
    emit $channel [list meta netlist_type map]
    emit $channel [list meta quartus_version $::quartus(version)]
    emit $channel [list meta generated_unix_seconds [clock seconds]]

    foreach_in_collection node [get_atom_nodes] {
        set name [atom_info_or_error $channel $node NAME node had_error]
        set type [atom_info_or_error $channel $node TYPE node had_error]
        set encrypted [atom_info_or_error $channel $node BOOL_ENCRYPTED node had_error]
        if {$name eq "" || $type eq "" || $encrypted eq ""} {
            emit $channel [list unavailable incomplete_node_identity $node $name $type $encrypted]
            set had_error 1
        }
        emit $channel [list node $node $type $encrypted $name]

        if {$encrypted eq "1" || [string equal -nocase $encrypted "true"]} {
            emit $channel [list unavailable encrypted_node $node $name]
            set had_error 1
            continue
        }

        if {[catch {get_atom_iports -node $node} iports]} {
            emit $channel [list error iports $node $iports]
            set had_error 1
            set iports {}
        }
        foreach port $iports {
            set ptype [port_info_or_error $channel $node iport $port type iport had_error]
            set pindex [port_info_or_error $channel $node iport $port literal_index iport had_error]
            set pencrypted [port_info_or_error $channel $node iport $port IS_ENCRYPTED iport had_error]
            set fanin [port_info_or_error $channel $node iport $port fanin iport had_error]
            if {$pindex eq ""} { set pindex -1 }
            if {$ptype eq "" || $pencrypted eq ""} {
                emit $channel [list unavailable incomplete_iport_identity $node $port $ptype $pindex $pencrypted]
                set had_error 1
            }
            emit $channel [list port $node iport $port $ptype $pindex "" $pencrypted]
            if {[catch {endpoint_pairs $fanin} fanin_pairs]} {
                emit $channel [list unavailable ambiguous_fanin $node $port $fanin]
                set had_error 1
                set fanin_pairs {}
            }
            if {[llength $fanin_pairs] > 1} {
                emit $channel [list unavailable multiple_fanin $node $port $fanin]
                set had_error 1
            }
            foreach pair $fanin_pairs {
                set source_node [lindex $pair 0]
                set source_port [lindex $pair 1]
                if {$source_node ne "-1" && $source_port ne "-1"} {
                    emit $channel [list edge $source_node $source_port $node $port fanin]
                }
            }
        }

        if {[catch {get_atom_oports -node $node} oports]} {
            emit $channel [list error oports $node $oports]
            set had_error 1
            set oports {}
        }
        foreach port $oports {
            set ptype [port_info_or_error $channel $node oport $port type oport had_error]
            set pindex [port_info_or_error $channel $node oport $port literal_index oport had_error]
            set pname [port_info_or_error $channel $node oport $port name oport had_error]
            set pencrypted [port_info_or_error $channel $node oport $port IS_ENCRYPTED oport had_error]
            set fanout [port_info_or_error $channel $node oport $port fanout oport had_error]
            if {$pindex eq ""} { set pindex -1 }
            if {$ptype eq "" || $pencrypted eq ""} {
                emit $channel [list unavailable incomplete_oport_identity $node $port $ptype $pindex $pencrypted]
                set had_error 1
            }
            emit $channel [list port $node oport $port $ptype $pindex $pname $pencrypted]
            if {[catch {endpoint_pairs $fanout} fanout_pairs]} {
                emit $channel [list unavailable ambiguous_fanout $node $port $fanout]
                set had_error 1
                set fanout_pairs {}
            }
            foreach pair $fanout_pairs {
                set destination_node [lindex $pair 0]
                set destination_port [lindex $pair 1]
                if {$destination_node ne "-1" && $destination_port ne "-1"} {
                    emit $channel [list edge $node $port $destination_node $destination_port fanout]
                }
            }
        }
    }

    # Optional audit dump.  Its text is never parsed as connectivity evidence.
    if {[catch {write_atom_netlist -verilog -file $output_vo} vo_error]} {
        set vo_status "unavailable"
    } else {
        set vo_status "captured"
    }
    emit $channel [list meta optional_vo_status $vo_status]
    if {$vo_error ne ""} {
        emit $channel [list note optional_vo_error $vo_error]
    }
    emit $channel [list meta connectivity_complete [expr {!$had_error}]]
} capture_error capture_options]} {
    catch {emit $channel [list error capture "" $capture_error]}
    catch {close $channel}
    if {$netlist_loaded} { catch {unload_atom_netlist} }
    if {$project_opened} { catch {project_close} }
    catch {file delete -force $tmp_tsv}
    return -options $capture_options $capture_error
}

close $channel
if {$netlist_loaded} { unload_atom_netlist }
if {$project_opened} { project_close }
file rename $tmp_tsv $output_tsv

if {$had_error} {
    fail "post-map atom connectivity was hidden, ambiguous, encrypted, or unavailable"
}
post_message -type info "DUAL18_ATOM_ROUTE_CAPTURE id=$capture_id tsv=$output_tsv vo=$vo_status"
