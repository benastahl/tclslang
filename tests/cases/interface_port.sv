// Interface ports, with and without a modport.
interface bus_if;
    logic       valid;
    logic [7:0] data;
    modport master (output valid, output data);
    modport slave  (input  valid, input  data);
endinterface

module producer(bus_if.master bus, input logic clk);
endmodule

module consumer(bus_if bus);
endmodule

module top(input logic clk);
    bus_if the_bus();
    producer u_prod (.bus(the_bus), .clk(clk));
    consumer u_cons (.bus(the_bus.slave));
endmodule
