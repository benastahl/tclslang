# Prints the ports, cells and connections of every top module in a design.
#
#   tclsh examples/dump_hierarchy.tcl design.sv [more.sv ...]
#
# With no arguments, dumps each design in tests/cases.

set here [file dirname [file normalize [info script]]]
if {[info exists env(TCLSLANG_LIB)]} {
    load $env(TCLSLANG_LIB)
} else {
    load [file join $here .. build libtclslang.so]
}

proc describe_driver {d} {
    if {$d eq ""} { return "(unconnected)" }
    switch -- [$d kind] {
        const     {
            if {[$d expr] eq [$d const]} { return [$d const] }
            return "[$d expr] = [$d const]"
        }
        var - net { return "[$d expr]  ([$d kind] [$d data_type])" }
        expr      { return "[$d expr]  (expression, [$d data_type])" }
        interface {
            set text "[$d name]  (interface"
            if {[$d modport] ne ""} { append text ", modport [$d modport]" }
            return "$text)"
        }
    }
}

proc dump_instance {inst depth} {
    set pad [string repeat "  " $depth]
    puts "${pad}[$inst name] : [$inst ref_name]    ([$inst hier_path])"

    foreach conn [$inst get_connections] {
        set port [$conn get_port]
        set dir [expr {[$port direction] ne "" ? [$port direction] : [$port kind]}]
        puts [format "%s  .%-12s %-7s <- %s" $pad [$conn name] $dir \
                  [describe_driver [$conn get_driver]]]
    }
    foreach cell [$inst get_cells] {
        dump_instance $cell [expr {$depth + 1}]
    }
}

proc dump_design {files} {
    if {[catch {slang_parse {*}$files} tree]} {
        puts $tree
        return
    }
    foreach top [$tree top_modules] {
        set mod [$tree get_module $top]
        puts "module $top"
        foreach p [$mod get_ports] {
            if {[$p kind] eq "interface"} {
                puts "  port [$p name]: interface [$p interface]"
            } else {
                puts [format "  port %-14s %-7s %s" [$p name] [$p direction] [$p data_type]]
            }
        }
        foreach cell [$mod get_cells] {
            dump_instance $cell 1
        }
    }
    $tree destroy
}

set designs $argv
if {[llength $designs] == 0} {
    set designs [lsort [glob -directory [file join $here .. tests cases] *.sv *.v multifile]]
}

foreach design $designs {
    puts "=== [file tail $design]"
    if {[file isdirectory $design]} {
        dump_design [lsort [glob -directory $design *.sv *.v]]
    } else {
        dump_design [list $design]
    }
    puts ""
}
