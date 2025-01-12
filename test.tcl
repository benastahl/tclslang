load ./build/libtclslang.so

set tree [slang_parse "./verilog_tests/test4.v"]

set module [$tree get_module "foo"]

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
    puts "dimension: [$p dimensions]"
    puts "--------------------------"
}
