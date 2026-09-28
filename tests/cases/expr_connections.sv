// Connections whose driver is not a single plain signal.
module leaf(input logic [1:0] a, output logic [1:0] y);
endmodule

module top(input logic [7:0] bus, input logic x, input logic z);
    logic [1:0] v_out, v_out2, v_out3;
    wire  [1:0] n_out;

    leaf u_and    (.a({1'b0, x & z}), .y(v_out));   // expression in, variable out
    leaf u_concat (.a({x, z}),        .y(n_out));   // concatenation in, net out
    leaf u_select (.a(bus[3:2]),      .y());        // part-select in, unconnected out
    leaf u_bit    (.a({1'b0, bus[5]}), .y(v_out2));
    leaf u_open   (.a(),              .y(v_out3));   // unconnected input
endmodule
