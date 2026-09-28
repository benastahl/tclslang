module top(input logic [3:0] in, output logic parity);
    leaf u_leaf (.a(in), .y(parity));
endmodule
