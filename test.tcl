load ./build/libtclslang.so

set tree [slang_parse "test.sv" "test2.sv"]

puts $tree

set module [$tree get_module "memory"]
set module2 [$tree get_module "memory2"]

puts $module
puts $module2

puts [$module name]
puts [$module2 name]


set ports [$module get_ports]

puts "\n\n"
puts "module1 ports!"
puts $ports
foreach p $ports {
    puts "full port string: [$p string]"
    puts "port [$p portType] [$p direction] [$p dataType]"
    set ident_list [$p identifiers]

    puts "dimension: [$p startDim]:[$p endDim]"

    puts ""
    puts "identifiers:"
    foreach i $ident_list {
        puts $i
    }
    puts "--------------------------"
}


set ports2 [$module2 get_ports]

puts "\n\n"
puts "module2 ports!"
puts $ports2
foreach p $ports2 {
    puts "full port string: [$p string]"
    puts "port [$p portType] [$p direction] [$p dataType]"
    set ident_list [$p identifiers]

    puts "dimension: [$p startDim]:[$p endDim]"

    puts ""
    puts "identifiers:"
    foreach i $ident_list {
        puts $i
    }
    puts "--------------------------"
}
