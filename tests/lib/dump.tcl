# Handle-free text dump of an elaborated design, used by the golden tests.
#
# Only names and attributes are printed, never handle IDs (port12, driver3),
# because handle numbering depends on call order.

namespace eval dump {}

proc dump::driver {d} {
    if {$d eq ""} {
        return "unconnected"
    }
    switch -- [$d type] {
        const     { set desc "const [$d const]" }
        var       { set desc "var [$d name] : [$d data_type]" }
        net       { set desc "net [$d name] : [$d data_type] ([$d net_type])" }
        expr      { return "expr {[$d expr]} : [$d data_type]" }
        interface {
            set desc "interface [$d name]"
            if {[$d modport] ne ""} { append desc " modport [$d modport]" }
            return $desc
        }
        default   { error "unknown driver type [$d type]" }
    }
    # Show the connection text when it is more than the bare name/value,
    # e.g. a select (bus[3:2]) or hierarchical reference (ifc.sig).
    set text [$d expr]
    set plain [expr {[$d type] eq "const" ? [$d const] : [$d name]}]
    if {$text ne $plain} {
        append desc " via {$text}"
    }
    return $desc
}

proc dump::port {p} {
    set line [join [concat port [$p name] [$p direction] [$p portType]]]
    if {[llength [$p dimensions]]} {
        append line " dims=[$p dimensions]"
    }
    return $line
}

proc dump::cell {c indent} {
    set pad [string repeat "  " $indent]
    set out "${pad}cell [$c name]\n"
    foreach conn [$c get_connections] {
        set p [$conn get_port]
        set dir [expr {[$p direction] ne "" ? [$p direction] : [$p portType]}]
        append out "${pad}  conn [$conn name] ($dir) <- [dump::driver [$conn get_driver]]\n"
    }
    foreach child [$c get_cells] {
        append out [dump::cell $child [expr {$indent + 1}]]
    }
    return $out
}

# Parses `files`, then dumps module `top` and everything below it.
proc dump::design {files {top top}} {
    if {[catch {slang_parse {*}$files} tree]} {
        return "error: $tree\n"
    }
    set mod [$tree get_module $top]
    if {$mod eq ""} {
        return "error: module \"$top\" not found\n"
    }
    set out "module [$mod name]\n"
    foreach p [$mod get_ports] {
        append out "  [dump::port $p]\n"
    }
    foreach c [$mod get_cells] {
        append out [dump::cell $c 1]
    }
    return $out
}
