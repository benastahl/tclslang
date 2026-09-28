// Valid design that elaborates with a warning: u_sub's port is never connected.
module sub(input logic a);
endmodule

module top;
    sub u_sub ();
endmodule
