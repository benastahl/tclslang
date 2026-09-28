load [file join [file dirname [info script]] .. build libtclslang.so]

# Usage: tclsh examples/dump_hierarchy.tcl [file.sv ...]
set files $argv
if {[llength $files] == 0} {
    set files [glob [file join [file dirname [info script]] .. tests cases *.v]]
}

foreach file $files {
    puts "==============================="
    puts "Parsing file: $file"
    puts "==============================="

    set tree [slang_parse $file]
    set module [$tree get_module top]
    if {$module eq ""} {  # checks for null if module name not found
        continue
    }

    set ports [$module get_ports]
    puts "ports: $ports"

    foreach p $ports {
        puts "port name: [$p name]"
        puts "port type: [$p portType]"
        puts "port direction: [$p direction]"
        puts "port declared type: [$p type]"
        puts "port dimension type: [$p dimType]"
        puts "dimension: [$p dimensions]"
        puts "--------------------------"
    }

    set cells [$module get_cells]
    puts "cells: $cells"

    foreach c $cells {
        puts "cell name: [$c name]"
        set conns [$c get_connections]

        foreach conn $conns {
            set d [$conn get_driver]
            set p [$conn get_port]

            puts "   conn name: [$conn name]"
            puts "   conn port: [$conn get_port]"
            puts "   port name: [$p name]"
            puts "   port type: [$p portType]"
            puts "   port direction: [$p direction]"
            puts "   port declared type: [$p type]"
            puts "   port dimension type: [$p dimType]"
            puts "   dimension: [$p dimensions]"
            puts ""

            set driver [$conn get_driver]
            puts "   conn driver: $driver"
            if {$driver eq ""} {
                continue
            }
            set driverType [$d type]

            if {$driverType eq "var"} {
                puts "   driver name: [$d name]"
                puts "   driver type: [$d type]"
                puts "   driver data_type: [$d data_type]"

            } elseif {$driverType eq "net"} {
                puts "   driver name: [$d name]"
                puts "   driver type: [$d type]"
                puts "   driver data_type: [$d data_type]"
                puts "   driver net_type: [$d net_type]"

            } elseif {$driverType eq "const"} {
                puts "   driver name: [$d name]"
                puts "   const: [$d const]"
            }

            puts "  ----------------------------"
        }
        puts "--------------------------"
    }

    puts "\n\n"
}
