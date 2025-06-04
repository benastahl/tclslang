// Simple adder module
module adder (
    input logic [3:0] a,
    input logic [3:0] b,
    output logic [4:0] sum
);
    assign sum = a + b;
endmodule

// Top module, with ports and an instance inside
module top (
    input logic [3:0] a,
    input logic [3:0] b,
    output logic [4:0] sum
);

    // Instance of adder inside top
    adder my_adder (
        .a(a),    // directly connect top ports to adder ports
        .b(b),
        .sum(sum)
    );

endmodule

// System module that instantiates top
module system;

    // Wires to connect to top
    logic [3:0] sys_a;
    logic [3:0] sys_b;
    logic [4:0] sys_sum;

    // Instance of top, with port connections
    top top_instance (
        .a(sys_a),    // Connect sys_a to top's input a
        .b(sys_b),    // Connect sys_b to top's input b
        .sum(sys_sum) // Connect sys_sum to top's output sum
    );

endmodule
