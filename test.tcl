load ./build/libtclslang.so

set tree [slang_parse "./verilog_tests/test5.v"]

set module [$tree get_module "example_module"]

set ports [$module get_ports]

puts "\n\n"
puts "module ports!"
puts $ports
foreach p $ports {
    puts "port name: [$p name]"
    puts "port type: [$p portType]"
    puts "port direction: [$p direction]"
    puts "port declared type: [$p type]"
    puts "port dimension type: [$p dimType]"
    puts "dimension: [$p startDim]:[$p endDim]"
    puts "--------------------------"
}
